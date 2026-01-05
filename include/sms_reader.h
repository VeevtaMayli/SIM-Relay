#ifndef SMS_READER_H
#define SMS_READER_H

#include <Arduino.h>
#include "config.h"
#include <TinyGsmClient.h>

// Multi-part SMS metadata (from UDH)
struct SmsPartInfo {
    bool isMultiPart;    // True if this is part of a concatenated SMS
    uint16_t refNumber;  // Reference number (same for all parts)
    uint8_t totalParts;  // Total number of parts
    uint8_t partNumber;  // This part's number (1-based)

    SmsPartInfo() : isMultiPart(false), refNumber(0), totalParts(1), partNumber(1) {}
};

struct SmsMessage {
    int index;           // SMS index in SIM memory
    String sender;       // Sender phone number or alphanumeric name (UTF-8)
    String text;         // Message text (UTF-8 decoded)
    String timestamp;    // Date and time from SMS (YYYY-MM-DD HH:MM:SS)
    SmsPartInfo partInfo; // Multi-part SMS metadata

    SmsMessage() : index(-1) {}

    bool isValid() const { return index >= 0; }
};

class SmsReader {
public:
    SmsReader(TinyGsm& modem);

    // Initialization
    bool init();

    // SMS operations
    bool hasNewSms();
    bool getUnreadSmsList(int* indices, int maxCount, int& count);
    SmsMessage readSms(int index);
    bool deleteSms(int index);

private:
    TinyGsm& modem;
    bool initialized;

    // PDU parsing
    bool parsePduResponse(const String& response, SmsMessage& sms);
    uint8_t hexToByte(char high, char low);

    // Sender decoding (phone number or alphanumeric)
    String decodeSender(const String& pdu, int& pos, int senderLen, uint8_t typeOfAddr);
    String decodePhoneNumber(const String& pdu, int& pos, int digitCount);
    String decodeAlphanumeric(const String& pdu, int& pos, int senderLen);

    // Timestamp parsing
    String decodeTimestamp(const String& pdu, int& pos);

    // UDH (User Data Header) parsing for multi-part SMS
    bool parseUdh(const uint8_t* data, int udhLen, SmsPartInfo& partInfo);

    // Text decoding
    String decodeUserData(const String& pdu, int& pos, uint8_t dcs, int udl, bool hasUdh, SmsPartInfo& partInfo);
    String decodeGsm7bit(const uint8_t* data, int len, int paddingBits);
    String decodeUcs2(const uint8_t* data, int len);
    String decodeUcs2Hex(const String& hexStr); // Legacy hex string decoder
};

#endif // SMS_READER_H
