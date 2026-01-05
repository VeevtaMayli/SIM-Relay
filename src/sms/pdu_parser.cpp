#include "sms/pdu_parser.h"
#include "sms/text_decoder.h"
#include "config.h"

bool PduParser::parse(const String& pduHex, SmsMessage& out) {
    if (pduHex.length() < 20) {
        DEBUG_PRINTLN("ERROR: PDU too short");
        return false;
    }

    int pos = 0;

    // 1. SMSC length (Service Center Address)
    uint8_t smscLen = hexToByte(pduHex.charAt(pos), pduHex.charAt(pos + 1));
    pos += 2;
    pos += smscLen * 2; // Skip SMSC address

    // 2. PDU Type (first octet) - determines message format
    uint8_t pduType = hexToByte(pduHex.charAt(pos), pduHex.charAt(pos + 1));
    pos += 2;

    // Check UDHI (User Data Header Indicator) flag
    bool hasUdh = (pduType & PduConst::UDHI_FLAG) != 0;

    DEBUG_PRINTF("PDU Type: 0x%02X, UDHI: %d\n", pduType, hasUdh);

    // 3. Sender Address Length (in digits/characters)
    uint8_t senderLen = hexToByte(pduHex.charAt(pos), pduHex.charAt(pos + 1));
    pos += 2;

    // 4. Type of Address
    uint8_t typeOfAddr = hexToByte(pduHex.charAt(pos), pduHex.charAt(pos + 1));
    pos += 2;

    // 5. Sender Address
    out.sender = decodeSender(pduHex, pos, senderLen, typeOfAddr);

    // 6. Protocol Identifier (PID)
    pos += 2; // Skip PID

    // 7. Data Coding Scheme (DCS)
    uint8_t dcs = hexToByte(pduHex.charAt(pos), pduHex.charAt(pos + 1));
    pos += 2;

    // 8. Timestamp (7 octets in semi-octet format)
    out.timestamp = decodeTimestamp(pduHex, pos);

    // 9. User Data Length (UDL)
    uint8_t udl = hexToByte(pduHex.charAt(pos), pduHex.charAt(pos + 1));
    pos += 2;

    // 10. User Data (UD) - may contain UDH if UDHI flag is set
    out.text = decodeUserData(pduHex, pos, dcs, udl, hasUdh, out.partInfo);

    return out.sender.length() > 0 && out.text.length() > 0;
}

uint8_t PduParser::hexToByte(char high, char low) {
    uint8_t h = (high >= 'A') ? (high - 'A' + 10) : (high - '0');
    uint8_t l = (low >= 'A') ? (low - 'A' + 10) : (low - '0');
    return (h << 4) | l;
}

String PduParser::decodeSender(const String& pdu, int& pos, int senderLen, uint8_t typeOfAddr) {
    // Type of Address format: bit 7 = extension, bits 6-4 = type, bits 3-0 = numbering plan
    // Type: 000 = unknown, 001 = international, 010 = national, 101 = alphanumeric

    // Check for alphanumeric sender
    if ((typeOfAddr & PduConst::TOA_TYPE_MASK) == PduConst::TOA_ALPHANUMERIC) {
        // Alphanumeric sender (e.g., "MegaFon", "Google")
        DEBUG_PRINTLN("Sender type: Alphanumeric");
        return decodeAlphanumeric(pdu, pos, senderLen);
    } else {
        // Phone number
        DEBUG_PRINTLN("Sender type: Phone number");
        return decodePhoneNumber(pdu, pos, senderLen);
    }
}

String PduParser::decodePhoneNumber(const String& pdu, int& pos, int digitCount) {
    // Phone number in semi-octet format (nibbles swapped)
    // Example: "79123456789" -> 97 21 43 65 87 F9
    String phone = "";
    int byteCount = (digitCount + 1) / 2;

    for (int i = 0; i < byteCount; i++) {
        uint8_t byte = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
        pos += 2;

        char low = (byte & PduConst::NIBBLE_LOW) + '0';
        char high = (byte >> 4) == PduConst::NIBBLE_LOW ? '\0' : ((byte >> 4) + '0');

        phone += low;
        if (high != '\0') {
            phone += high;
        }
    }

    return '+' + phone; // Add + prefix for international format
}

String PduParser::decodeAlphanumeric(const String& pdu, int& pos, int senderLen) {
    // Alphanumeric sender is encoded in GSM 7-bit
    // senderLen = number of characters * 2 (in semi-octet units for alphanumeric)
    int charCount = (senderLen + 1) / 2;  // Convert to actual character count
    int byteCount = (charCount * 7 + 7) / 8;  // Bytes needed for packed GSM 7-bit

    // Use static buffer to save stack space on MCU
    static uint8_t buffer[50];
    for (int i = 0; i < byteCount && i < 50; i++) {
        buffer[i] = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
        pos += 2;
    }

    return TextDecoder::decodeGsm7bit(buffer, charCount, 0);
}

String PduParser::decodeTimestamp(const String& pdu, int& pos) {
    // Timestamp: 7 octets in semi-octet format
    // Format: YY MM DD HH MM SS TZ
    // Each octet has nibbles swapped (e.g., 62 = 26)

    char buffer[30];
    uint8_t values[7];

    for (int i = 0; i < 7; i++) {
        uint8_t byte = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
        pos += 2;
        values[i] = ((byte & PduConst::NIBBLE_LOW) * 10) + (byte >> 4);
    }

    // Parse timezone (quarter-hours from GMT)
    // Bit 7 of TZ byte = sign (0=positive, 1=negative)
    uint8_t tzByte = hexToByte(pdu.charAt(pos - 2), pdu.charAt(pos - 1));
    bool tzNegative = (tzByte & 0x08) != 0; // Bit 3 of high nibble
    int tzQuarters = values[6];
    int tzHours = tzQuarters / 4;
    int tzMinutes = (tzQuarters % 4) * 15;

    // Format: YYYY-MM-DD HH:MM:SS±HH:MM
    snprintf(buffer, sizeof(buffer), "20%02d-%02d-%02d %02d:%02d:%02d%c%02d:%02d",
             values[0], values[1], values[2], values[3], values[4], values[5],
             tzNegative ? '-' : '+', tzHours, tzMinutes);

    return String(buffer);
}

bool PduParser::parseUdh(const uint8_t* data, int udhLen, SmsPartInfo& partInfo) {
    // UDH (User Data Header) format:
    // - IEI (1 byte): Information Element Identifier
    // - IEDL (1 byte): IE Data Length
    // - IE Data (IEDL bytes)

    int pos = 0;
    while (pos < udhLen) {
        uint8_t iei = data[pos++];
        uint8_t iedl = data[pos++];

        if (iei == PduConst::IEI_CONCAT_8BIT) {
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
        } else if (iei == PduConst::IEI_CONCAT_16BIT) {
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

String PduParser::decodeUserData(const String& pdu, int& pos, uint8_t dcs, int udl,
                                   bool hasUdh, SmsPartInfo& partInfo) {
    // Use static buffer to save stack space on MCU
    static uint8_t buffer[200];

    uint8_t encoding = dcs & PduConst::DCS_ENCODING_MASK;
    int udhLen = 0;

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
    }

    if (encoding == PduConst::DCS_UCS2) {
        // UCS-2 (16-bit Unicode)
        int textByteCount = udl - udhLen;

        for (int i = 0; i < textByteCount && i < 200; i++) {
            buffer[i] = hexToByte(pdu.charAt(pos), pdu.charAt(pos + 1));
            pos += 2;
        }

        return TextDecoder::decodeUcs2(buffer, textByteCount);
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

        return TextDecoder::decodeGsm7bit(buffer + (textStartBit / 8), textSeptets, textStartBit % 8);
    }
}
