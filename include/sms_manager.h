#ifndef SMS_MANAGER_H
#define SMS_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include <TinyGsmClient.h>
#include "sms/sms_types.h"

// AT+CMGL status codes (PDU mode)
namespace SmsStatus {
    constexpr int REC_UNREAD = 0;  // Received unread
    constexpr int REC_READ = 1;    // Received read
    constexpr int STO_UNSENT = 2;  // Stored unsent
    constexpr int STO_SENT = 3;    // Stored sent
    constexpr int ALL = 4;         // All messages
}

// SMS management via AT commands
class SmsManager {
public:
    SmsManager(TinyGsm& modem);

    // Initialize SMS subsystem (set PDU mode)
    bool init();

    // Check if any SMS messages exist
    bool hasNewSms();

    // Get list of all SMS indices
    bool getSmsList(int* indices, int maxCount, int& count);

    // Read SMS by index (returns parsed SmsMessage)
    SmsMessage readSms(int index);

    // Delete SMS by index
    bool deleteSms(int index);

private:
    TinyGsm& modem;
    bool initialized;
};

#endif // SMS_MANAGER_H
