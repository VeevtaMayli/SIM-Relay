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

    // Network management
    bool connectNetwork();
    bool isConnected();
    void reconnect();

    // Get modem instance
    TinyGsm& getModem() { return modem; }

private:
    TinyGsm modem;

    // Hardware initialization
    bool initializeHardware();
    bool powerOnModem();

    // Network helpers
    bool waitForNetwork(uint32_t timeout = 60000);
    bool connectGPRS();
};

#endif // MODEM_MANAGER_H
