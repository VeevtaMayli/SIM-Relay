#ifndef HTTP_SENDER_H
#define HTTP_SENDER_H

#include <Arduino.h>
#include "config.h"
#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include "sms/sms_types.h"
#include "ca_cert.h"

class HttpSender {
public:
    HttpSender();

    // Send SMS to server
    bool sendSmsToServer(const SmsMessage& sms);

    // Get last HTTP status code
    int getLastStatusCode() const { return lastStatusCode; }

    // Get last error message
    String getLastError() const { return lastError; }

private:
    int lastStatusCode;
    String lastError;

    // Create JSON payload from SMS
    String createJsonPayload(const SmsMessage& sms);
};

#endif // HTTP_SENDER_H
