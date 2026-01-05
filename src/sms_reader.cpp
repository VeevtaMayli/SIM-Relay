#include "sms_reader.h"

// GSM 7-bit default alphabet table (basic)
static const char GSM7_BASIC[] = {
    '@', '\xA3', '$', '\xA5', '\xE8', '\xE9', '\xF9', '\xEC', '\xF2', '\xC7', '\n', '\xD8', '\xF8', '\r', '\xC5', '\xE5',
    '\u0394', '_', '\u03A6', '\u0393', '\u039B', '\u03A9', '\u03A0', '\u03A8', '\u03A3', '\u0398', '\u039E', '\x1B', '\xC6', '\xE6', '\xDF', '\xC9',
    ' ', '!', '"', '#', '\xA4', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
    '\xA1', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '\xC4', '\xD6', '\xD1', '\xDC', '\xA7',
    '\xBF', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '\xE4', '\xF6', '\xF1', '\xFC', '\xE0'
};

// GSM 7-bit extended alphabet (escape sequences, accessed via 0x1B)
static const struct {
    uint8_t code;
    char replacement;
} GSM7_EXTENDED[] = {
    {0x0A, '\f'},  // Form feed
    {0x14, '^'},   // Caret
    {0x28, '{'},   // Left brace
    {0x29, '}'},   // Right brace
    {0x2F, '\\'},  // Backslash
    {0x3C, '['},   // Left bracket
    {0x3D, '~'},   // Tilde
    {0x3E, ']'},   // Right bracket
    {0x40, '|'},   // Pipe
    {0x65, '\u20AC'} // Euro sign
};

SmsReader::SmsReader(TinyGsm& m) : modem(m), initialized(false) {}

bool SmsReader::init() {
    DEBUG_PRINTLN("=== SMS Reader Initialization (PDU Mode) ===");

    // Set PDU mode (not text mode)
    modem.sendAT("+CMGF=0");
    if (modem.waitResponse() != 1) {
        DEBUG_PRINTLN("ERROR: Failed to set PDU mode");
        return false;
    }
    DEBUG_PRINTLN("PDU mode enabled");

    // Disable SMS status reports (optional, reduces clutter)
    modem.sendAT("+CSMP=17,167,0,0");
    if (modem.waitResponse() != 1) {
        DEBUG_PRINTLN("WARNING: Failed to set SMS parameters");
    }

    initialized = true;
    DEBUG_PRINTLN("SMS Reader initialized successfully");
    return true;
}

bool SmsReader::hasNewSms() {
    if (!initialized) {
        DEBUG_PRINTLN("ERROR: SMS Reader not initialized");
        return false;
    }

    String response = "";
    modem.sendAT("+CMGL=4"); // ALL messages in PDU mode
    modem.waitResponse(10000UL, response);

    return response.indexOf("+CMGL:") >= 0;
}

bool SmsReader::getUnreadSmsList(int* indices, int maxCount, int& count) {
    if (!initialized) {
        DEBUG_PRINTLN("ERROR: SMS Reader not initialized");
        return false;
    }

    count = 0;
    String response = "";

    // Get ALL messages (to handle retry scenarios where SMS becomes "READ" after failed send)
    modem.sendAT("+CMGL=4"); // 4 = ALL messages in PDU mode
    modem.waitResponse(10000UL, response);

    // Parse response for SMS indices
    // Format: +CMGL: <index>,<stat>,<alpha>,<length>\r\n<pdu>
    int pos = 0;
    while (count < maxCount && (pos = response.indexOf("+CMGL: ", pos)) >= 0) {
        pos += 7; // Skip "+CMGL: "
        int endPos = response.indexOf(',', pos);
        if (endPos > pos) {
            String indexStr = response.substring(pos, endPos);
            indices[count++] = indexStr.toInt();
            pos = endPos;
        }
    }

    DEBUG_PRINTF("Found %d SMS messages\n", count);
    return count > 0;
}

SmsMessage SmsReader::readSms(int index) {
    SmsMessage sms;

    if (!initialized) {
        DEBUG_PRINTLN("ERROR: SMS Reader not initialized");
        return sms;
    }

    DEBUG_PRINTF("Reading SMS at index %d\n", index);

    String response = "";
    modem.sendAT("+CMGR=" + String(index));
    if (modem.waitResponse(10000UL, response) != 1) {
        DEBUG_PRINTLN("ERROR: Failed to read SMS");
        return sms;
    }

    if (parsePduResponse(response, sms)) {
        sms.index = index;
        DEBUG_PRINTLN("SMS read successfully");
        DEBUG_PRINT("From: ");
        DEBUG_PRINTLN(sms.sender);
        DEBUG_PRINT("Time: ");
        DEBUG_PRINTLN(sms.timestamp);
        if (sms.partInfo.isMultiPart) {
            DEBUG_PRINTF("Part: %d/%d (ref: %d)\n", sms.partInfo.partNumber, sms.partInfo.totalParts, sms.partInfo.refNumber);
        }
        DEBUG_PRINT("Text: ");
        DEBUG_PRINTLN(sms.text);
    } else {
        DEBUG_PRINTLN("ERROR: Failed to parse PDU response");
    }

    return sms;
}

bool SmsReader::deleteSms(int index) {
    if (!initialized) {
        DEBUG_PRINTLN("ERROR: SMS Reader not initialized");
        return false;
    }

    DEBUG_PRINTF("Deleting SMS at index %d\n", index);

    modem.sendAT("+CMGD=" + String(index));
    if (modem.waitResponse() == 1) {
        DEBUG_PRINTLN("SMS deleted successfully");
        return true;
    }

    DEBUG_PRINTLN("ERROR: Failed to delete SMS");
    return false;
}

// ============================================
// PDU PARSING
// ============================================

bool SmsReader::parsePduResponse(const String& response, SmsMessage& sms) {
    // PDU response format:
    // +CMGR: <stat>,<alpha>,<length>\r\n<pdu hex string>

    DEBUG_PRINTLN("=== RAW PDU RESPONSE ===");
    DEBUG_PRINTLN(response);
    DEBUG_PRINTLN("=== END RAW RESPONSE ===");

    int cmgrPos = response.indexOf("+CMGR:");
    if (cmgrPos < 0) {
        return false;
    }

    // Find the PDU hex string (after first newline)
    int pduStart = response.indexOf('\n', cmgrPos);
    if (pduStart < 0) {
        return false;
    }
    pduStart++; // Skip newline

    int pduEnd = response.indexOf("\r\nOK", pduStart);
    if (pduEnd < 0) {
        pduEnd = response.length();
    }

    String pdu = response.substring(pduStart, pduEnd);
    pdu.trim();

    if (pdu.length() < 20) { // Minimum PDU length
        DEBUG_PRINTLN("ERROR: PDU too short");
        return false;
    }

    int pos = 0;

    // 1. SMSC length (Service Center Address)
    uint8_t smscLen = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
    pos += 2;
    pos += smscLen * 2; // Skip SMSC address

    // 2. PDU Type (first octet)
    uint8_t pduType = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
    pos += 2;

    // Check UDHI (User Data Header Indicator) flag (bit 6)
    bool hasUdh = (pduType & 0x40) != 0;

    DEBUG_PRINTF("PDU Type: 0x%02X, UDHI: %d\n", pduType, hasUdh);

    // 3. Sender Address Length (in digits)
    uint8_t senderLen = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
    pos += 2;

    // 4. Type of Address
    uint8_t typeOfAddr = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
    pos += 2;

    // 5. Sender Address
    sms.sender = decodeSender(pdu, pos, senderLen, typeOfAddr);

    // 6. Protocol Identifier (PID)
    pos += 2; // Skip PID

    // 7. Data Coding Scheme (DCS)
    uint8_t dcs = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
    pos += 2;

    // 8. Timestamp (7 octets in semi-octet format)
    sms.timestamp = decodeTimestamp(pdu, pos);

    // 9. User Data Length (UDL)
    uint8_t udl = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
    pos += 2;

    // 10. User Data (UD) - may contain UDH if UDHI flag is set
    sms.text = decodeUserData(pdu, pos, dcs, udl, hasUdh, sms.partInfo);

    return sms.sender.length() > 0 && sms.text.length() > 0;
}

uint8_t SmsReader::hexToByte(char high, char low) {
    uint8_t h = (high >= 'A') ? (high - 'A' + 10) : (high - '0');
    uint8_t l = (low >= 'A') ? (low - 'A' + 10) : (low - '0');
    return (h << 4) | l;
}

String SmsReader::decodeSender(const String& pdu, int& pos, int senderLen, uint8_t typeOfAddr) {
    // Type of Address format: bit 7 = extension, bits 6-4 = type, bits 3-0 = numbering plan
    // Type: 000 = unknown, 001 = international, 010 = national, 101 = alphanumeric

    // Correct check for alphanumeric: bits 6-4 should be 101 (0x50 masked)
    if ((typeOfAddr & 0x70) == 0x50) {
        // Alphanumeric sender (e.g., "MegaFon", "Google")
        DEBUG_PRINTLN("Sender type: Alphanumeric");
        return decodeAlphanumeric(pdu, pos, senderLen);
    } else {
        // Phone number
        DEBUG_PRINTLN("Sender type: Phone number");
        return decodePhoneNumber(pdu, pos, senderLen);
    }
}

String SmsReader::decodePhoneNumber(const String& pdu, int& pos, int digitCount) {
    // Phone number in semi-octet format (nibbles swapped)
    // Example: "79123456789" -> 97 21 43 65 87 F9
    String phone = "";
    int byteCount = (digitCount + 1) / 2;

    for (int i = 0; i < byteCount; i++) {
        uint8_t byte = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
        pos += 2;

        char low = (byte & 0x0F) + '0';
        char high = (byte >> 4) == 0x0F ? '\0' : ((byte >> 4) + '0');

        phone += low;
        if (high != '\0') {
            phone += high;
        }
    }

    return '+' + phone; // Add + prefix for international format
}

String SmsReader::decodeAlphanumeric(const String& pdu, int& pos, int senderLen) {
    // Alphanumeric sender is encoded in GSM 7-bit
    int byteCount = (senderLen * 7 + 7) / 8; // Round up

    // Use static buffer to save stack space on MCU
    static uint8_t buffer[50];
    for (int i = 0; i < byteCount && i < 50; i++) {
        buffer[i] = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
        pos += 2;
    }

    return decodeGsm7bit(buffer, senderLen, 0);
}

String SmsReader::decodeTimestamp(const String& pdu, int& pos) {
    // Timestamp: 7 octets in semi-octet format
    // Format: YY MM DD HH MM SS TZ
    // Each octet has nibbles swapped (e.g., 62 = 26)

    char buffer[20];
    uint8_t values[7];

    for (int i = 0; i < 7; i++) {
        uint8_t byte = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
        pos += 2;
        values[i] = ((byte & 0x0F) * 10) + (byte >> 4);
    }

    // Format: YYYY-MM-DD HH:MM:SS
    // Note: Timezone (values[6]) is ignored for simplicity
    snprintf(buffer, sizeof(buffer), "20%02d-%02d-%02d %02d:%02d:%02d",
             values[0], values[1], values[2], values[3], values[4], values[5]);

    return String(buffer);
}

bool SmsReader::parseUdh(const uint8_t* data, int udhLen, SmsPartInfo& partInfo) {
    // UDH (User Data Header) format:
    // - UDHL (1 byte): UDH length
    // - IEI (1 byte): Information Element Identifier
    // - IEDL (1 byte): IE Data Length
    // - IE Data (IEDL bytes)

    // For concatenated SMS (multi-part):
    // IEI = 0x00 (8-bit reference) or 0x08 (16-bit reference)

    int pos = 0;
    while (pos < udhLen) {
        uint8_t iei = data[pos++];
        uint8_t iedl = data[pos++];

        if (iei == 0x00) {
            // 8-bit concatenated SMS reference
            if (iedl >= 3) {
                partInfo.isMultiPart = true;
                partInfo.refNumber = data[pos];
                partInfo.totalParts = data[pos + 1];
                partInfo.partNumber = data[pos + 2];
                DEBUG_PRINTF("UDH: Multi-part SMS (8-bit ref=%d, part %d/%d)\n",
                    partInfo.refNumber, partInfo.partNumber, partInfo.totalParts);
                return true;
            }
        } else if (iei == 0x08) {
            // 16-bit concatenated SMS reference
            if (iedl >= 4) {
                partInfo.isMultiPart = true;
                partInfo.refNumber = (data[pos] << 8) | data[pos + 1];
                partInfo.totalParts = data[pos + 2];
                partInfo.partNumber = data[pos + 3];
                DEBUG_PRINTF("UDH: Multi-part SMS (16-bit ref=%d, part %d/%d)\n",
                    partInfo.refNumber, partInfo.partNumber, partInfo.totalParts);
                return true;
            }
        }

        pos += iedl; // Skip this IE
    }

    return false;
}

String SmsReader::decodeUserData(const String& pdu, int& pos, uint8_t dcs, int udl, bool hasUdh, SmsPartInfo& partInfo) {
    // Data Coding Scheme (DCS) determines encoding:
    // 0x00 = GSM 7-bit
    // 0x08 = UCS-2 (16-bit Unicode)
    // 0x04 = 8-bit data

    // Use static buffer to save stack space on MCU
    static uint8_t buffer[200];

    uint8_t encoding = dcs & 0x0C;
    int udhLen = 0;
    int textStartByte = 0;

    // If UDH is present, extract it first
    if (hasUdh) {
        // First byte of UD is UDHL (UDH Length)
        uint8_t udhl = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
        pos += 2;

        // Read UDH
        for (int i = 0; i < udhl && i < 200; i++) {
            buffer[i] = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
            pos += 2;
        }

        parseUdh(buffer, udhl, partInfo);

        udhLen = udhl + 1; // +1 for UDHL byte itself
        textStartByte = udhLen;
    }

    if (encoding == 0x08) {
        // UCS-2 (16-bit Unicode)
        int textByteCount = udl - udhLen;

        for (int i = 0; i < textByteCount && i < 200; i++) {
            buffer[i] = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
            pos += 2;
        }

        return decodeUcs2(buffer, textByteCount);
    } else {
        // GSM 7-bit (default)
        // UDL is in septets (characters), not bytes
        int totalSeptets = udl;
        int textSeptets = totalSeptets - (hasUdh ? ((udhLen * 8 + 6) / 7) : 0);
        int byteCount = (totalSeptets * 7 + 7) / 8;

        // Read all bytes
        for (int i = 0; i < byteCount && i < 200; i++) {
            buffer[i] = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
            pos += 2;
        }

        // Calculate padding bits after UDH
        int paddingBits = hasUdh ? (7 - ((udhLen * 8) % 7)) % 7 : 0;
        int textStartBit = hasUdh ? (udhLen * 8 + paddingBits) : 0;

        return decodeGsm7bit(buffer + (textStartBit / 8), textSeptets, textStartBit % 8);
    }
}

// ============================================
// TEXT DECODERS
// ============================================

String SmsReader::decodeGsm7bit(const uint8_t* data, int charCount, int paddingBits) {
    // GSM 7-bit alphabet unpacking with extended table support
    // Each character is 7 bits, packed into octets
    // paddingBits: used when text follows UDH header

    String result = "";
    result.reserve(charCount);

    int bitOffset = paddingBits;
    bool escapeNext = false;

    for (int i = 0; i < charCount; i++) {
        int byteIndex = (bitOffset / 8);
        int bitPos = bitOffset % 8;

        uint8_t char7bit;

        if (bitPos <= 1) {
            // Character fits in one byte
            char7bit = (data[byteIndex] >> bitPos) & 0x7F;
        } else {
            // Character spans two bytes
            uint8_t lowBits = data[byteIndex] >> bitPos;
            uint8_t highBits = (byteIndex + 1 < 200) ? (data[byteIndex + 1] << (8 - bitPos)) : 0;
            char7bit = (lowBits | highBits) & 0x7F;
        }

        // Handle escape sequences
        if (escapeNext) {
            // Look up in extended table
            bool found = false;
            for (int j = 0; j < sizeof(GSM7_EXTENDED) / sizeof(GSM7_EXTENDED[0]); j++) {
                if (GSM7_EXTENDED[j].code == char7bit) {
                    result += GSM7_EXTENDED[j].replacement;
                    found = true;
                    break;
                }
            }
            if (!found) {
                result += '?'; // Unknown extended character
            }
            escapeNext = false;
        } else if (char7bit == 0x1B) {
            // Escape character - next char is from extended table
            escapeNext = true;
        } else if (char7bit < 128) {
            result += GSM7_BASIC[char7bit];
        } else {
            result += '?'; // Unknown character
        }

        bitOffset += 7;
    }

    return result;
}

String SmsReader::decodeUcs2(const uint8_t* data, int byteCount) {
    // UCS-2 to UTF-8 conversion (supports surrogate pairs for emoji)
    String result = "";
    result.reserve(byteCount / 2);

    int i = 0;
    uint16_t prevHigh = 0;

    while (i < byteCount - 1) {
        uint16_t code = (data[i] << 8) | data[i + 1];
        i += 2;

        if (code >= 0xD800 && code <= 0xDBFF) {
            // High surrogate (first half of emoji)
            prevHigh = code;
            continue;
        } else if (code >= 0xDC00 && code <= 0xDFFF && prevHigh) {
            // Low surrogate - combine with high surrogate
            uint32_t fullCode = 0x10000 + (((prevHigh - 0xD800) << 10) | (code - 0xDC00));

            result += char(0xF0 | (fullCode >> 18));
            result += char(0x80 | ((fullCode >> 12) & 0x3F));
            result += char(0x80 | ((fullCode >> 6) & 0x3F));
            result += char(0x80 | (fullCode & 0x3F));
            prevHigh = 0;
            continue;
        }

        // Regular UCS-2 character
        if (code < 0x80) {
            result += char(code);
        } else if (code < 0x800) {
            result += char(0xC0 | (code >> 6));
            result += char(0x80 | (code & 0x3F));
        } else {
            result += char(0xE0 | (code >> 12));
            result += char(0x80 | ((code >> 6) & 0x3F));
            result += char(0x80 | (code & 0x3F));
        }
    }

    return result;
}

// Legacy hex string decoder (not used in PDU mode, kept for compatibility)
String SmsReader::decodeUcs2Hex(const String& hexStr) {
    if (hexStr.length() % 4 != 0) {
        return "";
    }

    String out = "";
    for (int i = 0; i + 3 < hexStr.length(); i += 4) {
        uint16_t code = (uint16_t)strtol(hexStr.substring(i, i + 4).c_str(), NULL, 16);

        if (code < 0x80) {
            out += char(code);
        } else if (code < 0x800) {
            out += char(0xC0 | (code >> 6));
            out += char(0x80 | (code & 0x3F));
        } else {
            out += char(0xE0 | (code >> 12));
            out += char(0x80 | ((code >> 6) & 0x3F));
            out += char(0x80 | (code & 0x3F));
        }
    }
    return out;
}
