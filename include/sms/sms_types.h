#ifndef SMS_TYPES_H
#define SMS_TYPES_H

#include <Arduino.h>

// Multi-part SMS metadata (extracted from UDH)
struct SmsPartInfo {
    bool isMultiPart;      // True if this is part of a concatenated SMS
    uint16_t refNumber;    // Reference number (same for all parts)
    uint8_t totalParts;    // Total number of parts
    uint8_t partNumber;    // This part's number (1-based)

    SmsPartInfo() : isMultiPart(false), refNumber(0), totalParts(1), partNumber(1) {}
};

// Complete SMS message (after PDU parsing and decoding)
struct SmsMessage {
    int index;             // SMS index in SIM memory (-1 = invalid)
    String sender;         // Sender phone number or alphanumeric name (UTF-8)
    String text;           // Message text (UTF-8 decoded)
    String timestamp;      // Date and time from SMS (YYYY-MM-DD HH:MM:SS)
    SmsPartInfo partInfo;  // Multi-part SMS metadata

    SmsMessage() : index(-1) {}

    bool isValid() const { return index >= 0; }
};

#endif // SMS_TYPES_H
