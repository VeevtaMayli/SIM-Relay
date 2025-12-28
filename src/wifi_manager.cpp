#include "wifi_manager.h"
#include "secrets.h"

bool WiFiManager::connect() {
    DEBUG_PRINTLN("=== WiFi Manager Initialization ===");
    DEBUG_PRINT("Connecting to WiFi: ");
    DEBUG_PRINTLN(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > WIFI_CONNECT_TIMEOUT) {
            DEBUG_PRINTLN();
            DEBUG_PRINTLN("ERROR: WiFi connection timeout");
            return false;
        }
        delay(500);
        DEBUG_PRINT(".");
    }

    DEBUG_PRINTLN();
    DEBUG_PRINTLN("WiFi connected successfully");
    DEBUG_PRINT("IP address: ");
    DEBUG_PRINTLN(WiFi.localIP());
    DEBUG_PRINTF("Signal strength (RSSI): %d dBm\n", WiFi.RSSI());

    return true;
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void WiFiManager::reconnect() {
    unsigned long currentTime = millis();

    // Prevent too frequent reconnection attempts
    if (currentTime - lastReconnectAttempt < RECONNECT_INTERVAL) {
        return;
    }

    lastReconnectAttempt = currentTime;
    DEBUG_PRINTLN("=== WiFi Reconnection Attempt ===");

    // Disconnect first if in a bad state
    WiFi.disconnect(true);
    delay(100);

    // Try to reconnect
    if (connect()) {
        DEBUG_PRINTLN("WiFi reconnection successful");
    } else {
        DEBUG_PRINTLN("WiFi reconnection failed, will retry...");
    }
}

int WiFiManager::getRSSI() {
    return WiFi.RSSI();
}

String WiFiManager::getLocalIP() {
    return WiFi.localIP().toString();
}