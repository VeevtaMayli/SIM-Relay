#include "sms_reader.h"

SmsReader::SmsReader(TinyGsm& m) : modem(m), initialized(false) {}

bool SmsReader::init() {
    DEBUG_PRINTLN("=== SMS Reader Initialization ===");

#if SMS_ENCODING_UCS2
    // Set UCS2 encoding for SMS
    modem.sendAT("+CSCS=\"UCS2\"");
    if (modem.waitResponse() != 1) {
        DEBUG_PRINTLN("ERROR: Failed to set UCS2 encoding");
        return false;
    }
    DEBUG_PRINTLN("UCS2 encoding enabled");
#endif

    // Set SMS text mode
    modem.sendAT("+CMGF=1");
    if (modem.waitResponse() != 1) {
        DEBUG_PRINTLN("ERROR: Failed to set text mode");
        return false;
    }
    DEBUG_PRINTLN("SMS text mode enabled");

    // Set SMS parameters
    modem.sendAT("+CSMP=17,167,0,8");
    if (modem.waitResponse() != 1) {
        DEBUG_PRINTLN("ERROR: Failed to set SMS parameters");
        return false;
    }
    DEBUG_PRINTLN("SMS parameters configured");

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
    modem.sendAT("+CMGL=\"REC UNREAD\"");
    modem.waitResponse(10000UL, response);

    // Check if response contains any SMS
    return response.indexOf("+CMGL:") >= 0;
}

bool SmsReader::getUnreadSmsList(int* indices, int maxCount, int& count) {
    if (!initialized) {
        DEBUG_PRINTLN("ERROR: SMS Reader not initialized");
        return false;
    }

    count = 0;
    String response = "";
    modem.sendAT("+CMGL=\"REC UNREAD\"");
    modem.waitResponse(10000UL, response);

    // Parse response for SMS indices
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

    DEBUG_PRINTF("Found %d unread SMS\n", count);
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

    if (parseSmsResponse(response, sms)) {
        sms.index = index;
        DEBUG_PRINTLN("SMS read successfully");
        DEBUG_PRINT("From: ");
        DEBUG_PRINTLN(sms.sender);
        DEBUG_PRINT("Text: ");
        DEBUG_PRINTLN(sms.text);
    } else {
        DEBUG_PRINTLN("ERROR: Failed to parse SMS response");
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

bool SmsReader::parseSmsResponse(const String& response, SmsMessage& sms) {
    // Response format: +CMGR: "REC UNREAD","sender",,"timestamp"\r\ntext

    int cmgrPos = response.indexOf("+CMGR:");
    if (cmgrPos < 0) {
        return false;
    }

    // Extract sender (between second and third quotes)
    int senderStart = response.indexOf('"', cmgrPos);
    senderStart = response.indexOf('"', senderStart + 1);
    senderStart = response.indexOf('"', senderStart + 1);
    int senderEnd = response.indexOf('"', senderStart + 1);

    if (senderStart > 0 && senderEnd > senderStart) {
        sms.sender = response.substring(senderStart + 1, senderEnd);
#if SMS_ENCODING_UCS2
        // Decode UCS2 encoded sender
        sms.sender = decodeUcs2(sms.sender);
#endif
    }

    // Extract timestamp (between fourth and fifth quotes)
    int timestampStart = response.indexOf('"', senderEnd + 1);
    timestampStart = response.indexOf('"', timestampStart + 1);
    int timestampEnd = response.indexOf('"', timestampStart + 1);

    if (timestampStart > 0 && timestampEnd > timestampStart) {
        sms.timestamp = response.substring(timestampStart + 1, timestampEnd);
    }

    // Extract text (after the header line)
    int textStart = response.indexOf('\n', cmgrPos);
    if (textStart > 0) {
        textStart++; // Skip newline
        // Find end of text (before "OK" or end of string)
        int textEnd = response.indexOf("\r\nOK", textStart);
        if (textEnd < 0) {
            textEnd = response.length();
        }

        sms.text = response.substring(textStart, textEnd);
        sms.text.trim();

#if SMS_ENCODING_UCS2
        // Decode UCS2 encoded text
        sms.text = decodeUcs2(sms.text);
#endif
    }

    return sms.sender.length() > 0 && sms.text.length() > 0;
}

String SmsReader::decodeUcs2(const String& ucs2) {
    // UCS2 to UTF-8 conversion (supports Cyrillic, emoji with surrogate pairs)
    String out = "";
    uint16_t prevHigh = 0;

    for (int i = 0; i < ucs2.length(); i += 4) {
        String hexChar = ucs2.substring(i, i + 4);
        uint16_t code = (uint16_t)strtol(hexChar.c_str(), NULL, 16);

        if (code >= 0xD800 && code <= 0xDBFF) {
            // High surrogate (first half of emoji)
            prevHigh = code;
            continue;
        } else if (code >= 0xDC00 && code <= 0xDFFF && prevHigh) {
            // Low surrogate - combine with high surrogate
            uint32_t fullCode = 0x10000 + (((prevHigh - 0xD800) << 10) | (code - 0xDC00));

            out += char(0xF0 | (fullCode >> 18));
            out += char(0x80 | ((fullCode >> 12) & 0x3F));
            out += char(0x80 | ((fullCode >> 6) & 0x3F));
            out += char(0x80 | (fullCode & 0x3F));
            prevHigh = 0;
            continue;
        }

        // Regular UCS2 character
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
