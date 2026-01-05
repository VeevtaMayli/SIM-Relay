#include <Arduino.h>
#include "config.h"
#include "utilities.h"
#include "modem_manager.h"
#include "wifi_manager.h"
#include "sms_manager.h"
#include "http_sender.h"
#include "sms/sms_concatenator.h"

// Global objects
ModemManager modemManager;  // For SMS operations via LTE modem
WiFiManager wifiManager;    // For HTTP operations via ESP32 WiFi
SmsManager* smsManager = nullptr;
HttpSender* httpSender = nullptr;
SmsConcatenator smsConcatenator;  // Multi-part SMS handler

// Timing variables
unsigned long lastSmsCheck = 0;
unsigned long lastNetworkCheck = 0;
unsigned long lastCleanup = 0;

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

    // Initialize SMS manager
    DEBUG_PRINTLN("Step 3: Initializing SMS manager...");
    smsManager = new SmsManager(modemManager.getModem());
    if (!smsManager->init()) {
        DEBUG_PRINTLN("FATAL ERROR: SMS manager initialization failed!");
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

    // Clean up old partial multi-part SMS every 60 seconds
    if (currentMillis - lastCleanup >= 60000) {
        lastCleanup = currentMillis;
        smsConcatenator.cleanup();
    }

    // Check for new SMS
    if (currentMillis - lastSmsCheck >= SMS_CHECK_INTERVAL) {
        lastSmsCheck = currentMillis;

        // Get list of SMS
        int indices[10]; // Maximum 10 SMS at once
        int count = 0;

        if (smsManager->getSmsList(indices, 10, count)) {
            DEBUG_PRINTF("\n>>> Found %d SMS messages <<<\n\n", count);

            // Track indices of parts to delete after successful send
            std::vector<int> partsToDelete;

            // Process each SMS
            for (int i = 0; i < count; i++) {
                DEBUG_PRINTF("--- Processing SMS %d/%d (Index: %d) ---\n", i + 1, count, indices[i]);

                // Read SMS
                SmsMessage sms = smsManager->readSms(indices[i]);

                if (sms.isValid()) {
                    // Add to concatenator (handles both single and multi-part SMS)
                    SmsMessage* completeSms = smsConcatenator.addPart(sms);

                    if (completeSms != nullptr) {
                        // We have a complete message (either single-part or all parts received)
                        DEBUG_PRINTLN("→ Sending complete message to server");
                        bool sendSuccess = httpSender->sendSmsToServer(*completeSms);

                        if (sendSuccess) {
                            DEBUG_PRINTLN("✓ SMS successfully sent to server");

#if SMS_DELETE_AFTER_SEND
                            // Mark this index for deletion
                            partsToDelete.push_back(sms.index);

                            // For multi-part SMS, also mark all other parts for deletion
                            // (they were already processed by concatenator)
                            if (sms.partInfo.isMultiPart) {
                                // Note: We only have the last part's index here
                                // Other parts are already in the list from previous iterations
                            }
#endif
                        } else {
                            DEBUG_PRINTLN("✗ Failed to send SMS to server");
                            DEBUG_PRINT("Error: ");
                            DEBUG_PRINTLN(httpSender->getLastError());
                            DEBUG_PRINTLN("SMS will be retried on next check");
                        }
                    } else {
                        // Part of multi-part SMS, waiting for more parts
                        DEBUG_PRINTLN("⏳ Part buffered, waiting for remaining parts");
#if SMS_DELETE_AFTER_SEND
                        // Delete this part from SIM immediately to free up space
                        // (it's already stored in RAM buffer)
                        partsToDelete.push_back(sms.index);
#endif
                    }
                } else {
                    DEBUG_PRINTLN("✗ Failed to read SMS");
                }

                DEBUG_PRINTLN();
            }

#if SMS_DELETE_AFTER_SEND
            // Delete all processed parts
            for (int idx : partsToDelete) {
                if (smsManager->deleteSms(idx)) {
                    DEBUG_PRINTF("✓ SMS %d deleted from SIM\n", idx);
                } else {
                    DEBUG_PRINTF("✗ Failed to delete SMS %d from SIM\n", idx);
                }
            }
#endif

            DEBUG_PRINTLN("--- SMS processing completed ---\n");
        }
    }

    // Small delay to prevent tight loop
    delay(100);
}
