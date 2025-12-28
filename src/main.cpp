#include <Arduino.h>
#include "config.h"
#include "utilities.h"
#include "modem_manager.h"
#include "wifi_manager.h"
#include "sms_reader.h"
#include "http_sender.h"

// Global objects
ModemManager modemManager;  // For SMS operations via LTE modem
WiFiManager wifiManager;    // For HTTP operations via ESP32 WiFi
SmsReader* smsReader = nullptr;
HttpSender* httpSender = nullptr;

// Timing variables
unsigned long lastSmsCheck = 0;
unsigned long lastNetworkCheck = 0;

void setup() {
    // Initialize serial monitor
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);

    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("    SIM-Relay: SMS Gateway Device");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN();

    // Initialize modem
    DEBUG_PRINTLN("Step 1: Initializing modem...");
    if (!modemManager.init()) {
        DEBUG_PRINTLN("FATAL ERROR: Modem initialization failed!");
        DEBUG_PRINTLN("System halted. Please restart the device.");
        while (true) {
            delay(1000);
        }
    }
    DEBUG_PRINTLN();

    // Connect to WiFi (for HTTP)
    DEBUG_PRINTLN("Step 2: Connecting to WiFi...");
    if (!wifiManager.connect()) {
        DEBUG_PRINTLN("FATAL ERROR: WiFi connection failed!");
        DEBUG_PRINTLN("System halted. Please restart the device.");
        while (true) {
            delay(1000);
        }
    }
    DEBUG_PRINTLN();

    // Initialize SMS reader
    DEBUG_PRINTLN("Step 3: Initializing SMS reader...");
    smsReader = new SmsReader(modemManager.getModem());
    if (!smsReader->init()) {
        DEBUG_PRINTLN("FATAL ERROR: SMS reader initialization failed!");
        DEBUG_PRINTLN("System halted. Please restart the device.");
        while (true) {
            delay(1000);
        }
    }
    DEBUG_PRINTLN();

    // Initialize HTTP sender (uses WiFi, not modem)
    DEBUG_PRINTLN("Step 4: Initializing HTTP sender...");
    httpSender = new HttpSender();
    DEBUG_PRINTLN("HTTP sender initialized (WiFi)");
    DEBUG_PRINTLN();

    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("   System Ready - Monitoring SMS...");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN();
}

void loop() {
    unsigned long currentMillis = millis();

    // Check WiFi connection periodically
    if (currentMillis - lastNetworkCheck >= NETWORK_CHECK_INTERVAL) {
        lastNetworkCheck = currentMillis;

        if (!wifiManager.isConnected()) {
            DEBUG_PRINTLN("WARNING: WiFi connection lost!");
            wifiManager.reconnect();
        }
    }

    // Check for new SMS
    if (currentMillis - lastSmsCheck >= SMS_CHECK_INTERVAL) {
        lastSmsCheck = currentMillis;

        // Get list of unread SMS
        int indices[10]; // Maximum 10 SMS at once
        int count = 0;

        if (smsReader->getUnreadSmsList(indices, 10, count)) {
            DEBUG_PRINTF("\n>>> Found %d unread SMS <<<\n\n", count);

            // Process each SMS
            for (int i = 0; i < count; i++) {
                DEBUG_PRINTF("--- Processing SMS %d/%d (Index: %d) ---\n", i + 1, count, indices[i]);

                // Read SMS
                SmsMessage sms = smsReader->readSms(indices[i]);

                if (sms.isValid()) {
                    // Send to server
                    bool sendSuccess = httpSender->sendSmsToServer(sms);

                    if (sendSuccess) {
                        DEBUG_PRINTLN("✓ SMS successfully sent to server");

#if SMS_DELETE_AFTER_SEND
                        // Delete SMS from SIM (only after successful send)
                        if (smsReader->deleteSms(sms.index)) {
                            DEBUG_PRINTLN("✓ SMS deleted from SIM");
                        } else {
                            DEBUG_PRINTLN("✗ Failed to delete SMS from SIM");
                        }
#endif
                    } else {
                        DEBUG_PRINTLN("✗ Failed to send SMS to server");
                        DEBUG_PRINT("Error: ");
                        DEBUG_PRINTLN(httpSender->getLastError());
                        DEBUG_PRINTLN("SMS will be retried on next check");
                    }
                } else {
                    DEBUG_PRINTLN("✗ Failed to read SMS");
                }

                DEBUG_PRINTLN();
            }

            DEBUG_PRINTLN("--- SMS processing completed ---\n");
        }
    }

    // Small delay to prevent tight loop
    delay(100);
}
