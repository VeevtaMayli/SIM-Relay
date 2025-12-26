#ifndef SMS_READER_H
#define SMS_READER_H

#include <Arduino.h>
#include <TinyGsmClient.h>
#include "config.h"

struct SmsMessage {
    int index;           // SMS index in SIM memory
    String sender;       // Sender phone number
    String text;         // Message text (UTF-8 decoded)
    String timestamp;    // Date and time from SMS

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

    // UCS2 decoding (from ReadSMS example)
    String decodeUcs2(const String& hexStr);

    // Helper to parse SMS response
    bool parseSmsResponse(const String& response, SmsMessage& sms);
};

#endif // SMS_READER_H
