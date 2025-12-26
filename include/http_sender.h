#ifndef HTTP_SENDER_H
#define HTTP_SENDER_H

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sms_reader.h"

class HttpSender {
public:
    HttpSender(TinyGsm& modem);

    // Send SMS to server
    bool sendSmsToServer(const SmsMessage& sms);

    // Get last HTTP status code
    int getLastStatusCode() const { return lastStatusCode; }

    // Get last error message
    String getLastError() const { return lastError; }

private:
    TinyGsm& modem;
    int lastStatusCode;
    String lastError;

    // Create JSON payload from SMS
    String createJsonPayload(const SmsMessage& sms);
};

#endif // HTTP_SENDER_H
