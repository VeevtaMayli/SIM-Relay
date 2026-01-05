#include "sms_manager.h"
#include "sms/pdu_parser.h"

SmsManager::SmsManager(TinyGsm& m) : modem(m), initialized(false) {}

bool SmsManager::init() {
    DEBUG_PRINTLN("=== SMS Manager Initialization (PDU Mode) ===");

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
    DEBUG_PRINTLN("SMS Manager initialized successfully");
    return true;
}

bool SmsManager::hasNewSms() {
    if (!initialized) {
        DEBUG_PRINTLN("ERROR: SMS Manager not initialized");
        return false;
    }

    String response = "";
    modem.sendAT("+CMGL=" + String(SmsStatus::ALL));
    modem.waitResponse(10000UL, response);

    return response.indexOf("+CMGL:") >= 0;
}

bool SmsManager::getSmsList(int* indices, int maxCount, int& count) {
    if (!initialized) {
        DEBUG_PRINTLN("ERROR: SMS Manager not initialized");
        return false;
    }

    count = 0;
    String response = "";

    // Get ALL messages
    modem.sendAT("+CMGL=" + String(SmsStatus::ALL));
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

SmsMessage SmsManager::readSms(int index) {
    SmsMessage sms;

    if (!initialized) {
        DEBUG_PRINTLN("ERROR: SMS Manager not initialized");
        return sms;
    }

    DEBUG_PRINTF("Reading SMS at index %d\n", index);

    String response = "";
    modem.sendAT("+CMGR=" + String(index));
    if (modem.waitResponse(10000UL, response) != 1) {
        DEBUG_PRINTLN("ERROR: Failed to read SMS");
        return sms;
    }

    DEBUG_PRINTLN("=== RAW AT RESPONSE ===");
    DEBUG_PRINTLN(response);
    DEBUG_PRINTLN("=== END RAW RESPONSE ===");

    // Extract PDU hex string from response
    // Format: +CMGR: <stat>,<alpha>,<length>\r\n<pdu>\r\nOK
    int cmgrPos = response.indexOf("+CMGR:");
    if (cmgrPos < 0) {
        DEBUG_PRINTLN("ERROR: Invalid AT response");
        return sms;
    }

    int pduStart = response.indexOf('\n', cmgrPos);
    if (pduStart < 0) {
        DEBUG_PRINTLN("ERROR: No PDU found");
        return sms;
    }
    pduStart++; // Skip newline

    int pduEnd = response.indexOf("\r\nOK", pduStart);
    if (pduEnd < 0) {
        pduEnd = response.length();
    }

    String pduHex = response.substring(pduStart, pduEnd);
    pduHex.trim();

    // Delegate parsing to PduParser
    if (PduParser::parse(pduHex, sms)) {
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
        DEBUG_PRINTLN("ERROR: Failed to parse PDU");
    }

    return sms;
}

bool SmsManager::deleteSms(int index) {
    if (!initialized) {
        DEBUG_PRINTLN("ERROR: SMS Manager not initialized");
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
