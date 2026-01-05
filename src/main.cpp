#include <Arduino.h>
#include "config.h"
#include "utilities.h"
#include "modem_manager.h"
#include "wifi_manager.h"
#include "sms_reader.h"
#include "http_sender.h"
#include <map>
#include <vector>

// ============================================
// MULTI-PART SMS CONCATENATOR
// ============================================

struct SmsPartBuffer {
    String sender;
    String timestamp;
    std::vector<String> parts;  // Indexed by partNumber-1
    uint8_t totalParts;
    unsigned long firstPartTime;  // millis() when first part arrived

    SmsPartBuffer() : totalParts(0), firstPartTime(0) {}
};

class SmsConcatenator {
private:
    std::map<uint16_t, SmsPartBuffer> partBuffers;  // Key: refNumber
    static const unsigned long PART_TIMEOUT = 300000; // 5 minutes

public:
    // Add a part and return concatenated SMS if all parts are received
    // Returns nullptr if more parts are needed
    SmsMessage* addPart(const SmsMessage& sms) {
        if (!sms.partInfo.isMultiPart) {
            // Single-part SMS, return as-is
            static SmsMessage result;
            result = sms;
            return &result;
        }

        uint16_t ref = sms.partInfo.refNumber;

        // Initialize buffer if this is the first part
        if (partBuffers.find(ref) == partBuffers.end()) {
            partBuffers[ref].sender = sms.sender;
            partBuffers[ref].timestamp = sms.timestamp;
            partBuffers[ref].totalParts = sms.partInfo.totalParts;
            partBuffers[ref].parts.resize(sms.partInfo.totalParts);
            partBuffers[ref].firstPartTime = millis();
        }

        SmsPartBuffer& buffer = partBuffers[ref];

        // Store this part (1-based indexing)
        int idx = sms.partInfo.partNumber - 1;
        if (idx >= 0 && idx < buffer.totalParts) {
            buffer.parts[idx] = sms.text;
        }

        // Check if all parts received
        bool allReceived = true;
        for (int i = 0; i < buffer.totalParts; i++) {
            if (buffer.parts[i].length() == 0) {
                allReceived = false;
                break;
            }
        }

        if (allReceived) {
            // Concatenate all parts
            static SmsMessage result;
            result.index = sms.index;  // Use last part's index
            result.sender = buffer.sender;
            result.timestamp = buffer.timestamp;
            result.text = "";
            for (int i = 0; i < buffer.totalParts; i++) {
                result.text += buffer.parts[i];
            }
            result.partInfo.isMultiPart = false;  // Mark as complete

            // Remove from buffer
            partBuffers.erase(ref);

            DEBUG_PRINTF("✓ Concatenated %d-part SMS (ref: %d)\n", buffer.totalParts, ref);
            return &result;
        }

        return nullptr;  // More parts needed
    }

    // Clean up old partial messages
    void cleanup() {
        unsigned long now = millis();
        for (auto it = partBuffers.begin(); it != partBuffers.end(); ) {
            if (now - it->second.firstPartTime > PART_TIMEOUT) {
                DEBUG_PRINTF("⚠ Timeout: Dropping incomplete multi-part SMS (ref: %d)\n", it->first);
                it = partBuffers.erase(it);
            } else {
                ++it;
            }
        }
    }
};

// Global objects
ModemManager modemManager;  // For SMS operations via LTE modem
WiFiManager wifiManager;    // For HTTP operations via ESP32 WiFi
SmsReader* smsReader = nullptr;
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

    // Clean up old partial multi-part SMS every 60 seconds
    if (currentMillis - lastCleanup >= 60000) {
        lastCleanup = currentMillis;
        smsConcatenator.cleanup();
    }

    // Check for new SMS
    if (currentMillis - lastSmsCheck >= SMS_CHECK_INTERVAL) {
        lastSmsCheck = currentMillis;

        // Get list of unread SMS
        int indices[10]; // Maximum 10 SMS at once
        int count = 0;

        if (smsReader->getUnreadSmsList(indices, 10, count)) {
            DEBUG_PRINTF("\n>>> Found %d SMS messages <<<\n\n", count);

            // Track indices of parts to delete after successful send
            std::vector<int> partsToDelete;

            // Process each SMS
            for (int i = 0; i < count; i++) {
                DEBUG_PRINTF("--- Processing SMS %d/%d (Index: %d) ---\n", i + 1, count, indices[i]);

                // Read SMS
                SmsMessage sms = smsReader->readSms(indices[i]);

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
                if (smsReader->deleteSms(idx)) {
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
