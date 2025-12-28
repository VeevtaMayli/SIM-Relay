#ifndef MODEM_MANAGER_H
#define MODEM_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "utilities.h"
#include <TinyGsmClient.h>

class ModemManager {
public:
    ModemManager();

    // Initialization
    bool init();

    // Get modem instance (used for SMS operations)
    TinyGsm& getModem() { return modem; }

    // TODO: GPRS fallback - uncomment when implementing WiFi fallback to GPRS
    // bool connectNetwork();
    // bool isConnected();
    // void reconnect();

private:
    TinyGsm modem;

    // Hardware initialization
    bool initializeHardware();
    bool powerOnModem();

    // TODO: GPRS fallback - uncomment when implementing WiFi fallback to GPRS
    // bool waitForNetwork(uint32_t timeout = 60000);
    // bool connectGPRS();
};

#endif // MODEM_MANAGER_H
