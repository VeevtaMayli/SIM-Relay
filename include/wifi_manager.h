#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include "config.h"

class WiFiManager {
public:
    // Initialize and connect to WiFi
    bool connect();

    // Check if WiFi is connected
    bool isConnected();

    // Attempt to reconnect if disconnected
    void reconnect();

    // Get current signal strength
    int getRSSI();

    // Get local IP address
    String getLocalIP();

private:
    unsigned long lastReconnectAttempt = 0;
    static const unsigned long RECONNECT_INTERVAL = 10000; // 10 seconds between reconnect attempts
};

#endif // WIFI_MANAGER_H