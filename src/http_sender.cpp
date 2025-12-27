#include "http_sender.h"

HttpSender::HttpSender(TinyGsm& m) : modem(m), lastStatusCode(0) {}

bool HttpSender::sendSmsToServer(const SmsMessage& sms) {
    if (!sms.isValid()) {
        lastError = "Invalid SMS message";
        DEBUG_PRINTLN("ERROR: Invalid SMS message");
        return false;
    }

    DEBUG_PRINTLN("=== Sending SMS to Server ===");
    DEBUG_PRINT("Server: ");
    DEBUG_PRINT(SERVER_HOST);
    DEBUG_PRINT(":");
    DEBUG_PRINTLN(SERVER_PORT);

    // Create HTTPS client (SSL/TLS)
    TinyGsmClientSecure client(modem);
    HttpClient http(client, SERVER_HOST, SERVER_PORT);

    // Set timeout
    http.setHttpResponseTimeout(HTTP_TIMEOUT);

    // Create JSON payload
    String jsonPayload = createJsonPayload(sms);
    DEBUG_PRINT("Payload: ");
    DEBUG_PRINTLN(jsonPayload);

    // Send POST request
    DEBUG_PRINTLN("Sending HTTP POST request...");
    http.beginRequest();
    int err = http.post(SERVER_PATH);
    if (err != 0) {
        lastError = "Connection failed, error code: " + String(err);
        DEBUG_PRINT("ERROR: ");
        DEBUG_PRINTLN(lastError);
        http.stop();
        return false;
    }

    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("X-API-Key", API_KEY);
    http.sendHeader("Content-Length", jsonPayload.length());
    http.endRequest();
    http.print(jsonPayload);

    // Get status code
    lastStatusCode = http.responseStatusCode();
    DEBUG_PRINT("HTTP Status Code: ");
    DEBUG_PRINTLN(lastStatusCode);

    // Read response body
    String responseBody = http.responseBody();
    DEBUG_PRINT("Response: ");
    DEBUG_PRINTLN(responseBody);

    // Close connection
    http.stop();

    // Check if successful (200 OK)
    if (lastStatusCode == 200) {
        DEBUG_PRINTLN("SMS sent to server successfully");
        return true;
    } else {
        lastError = "Server returned status: " + String(lastStatusCode);
        DEBUG_PRINT("ERROR: ");
        DEBUG_PRINTLN(lastError);
        return false;
    }
}

String HttpSender::createJsonPayload(const SmsMessage& sms) {
    // Create JSON document
    JsonDocument doc;

    // Add SMS fields
    doc["sender"] = sms.sender;
    doc["text"] = sms.text;
    doc["timestamp"] = sms.timestamp;

    // Serialize to string
    String output;
    serializeJson(doc, output);

    return output;
}
