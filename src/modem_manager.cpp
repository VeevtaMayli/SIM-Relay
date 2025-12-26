#include "modem_manager.h"

ModemManager::ModemManager() : modem(SerialAT) {}

bool ModemManager::init() {
    DEBUG_PRINTLN("=== Modem Manager Initialization ===");

    if (!initializeHardware()) {
        DEBUG_PRINTLN("ERROR: Hardware initialization failed");
        return false;
    }

    if (!powerOnModem()) {
        DEBUG_PRINTLN("ERROR: Modem power-on failed");
        return false;
    }

    DEBUG_PRINTLN("Modem initialization successful");
    return true;
}

bool ModemManager::initializeHardware() {
    DEBUG_PRINTLN("Initializing hardware pins...");

#ifdef BOARD_POWERON_PIN
    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
    DEBUG_PRINTLN("Board power-on pin set HIGH");
#endif

#ifdef MODEM_RESET_PIN
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
    DEBUG_PRINTLN("Modem reset pin configured");
#endif

#ifdef MODEM_DTR_PIN
    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_DTR_PIN, LOW);
    DEBUG_PRINTLN("DTR pin set LOW (modem awake)");
#endif

    return true;
}

bool ModemManager::powerOnModem() {
    DEBUG_PRINTLN("Powering on modem...");

    pinMode(BOARD_PWRKEY_PIN, OUTPUT);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH);
    delay(MODEM_POWERON_PULSE_WIDTH_MS);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);

    // Start serial communication with modem
    SerialAT.begin(MODEM_BAUDRATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    DEBUG_PRINTLN("Waiting for modem to start...");
    delay(3000);

    // Initialize modem
    if (!modem.init()) {
        DEBUG_PRINTLN("ERROR: Modem init() failed");
        return false;
    }

    String modemInfo = modem.getModemInfo();
    DEBUG_PRINT("Modem Info: ");
    DEBUG_PRINTLN(modemInfo);

    return true;
}

bool ModemManager::connectNetwork() {
    DEBUG_PRINTLN("=== Connecting to Network ===");

    if (!waitForNetwork()) {
        DEBUG_PRINTLN("ERROR: Network registration failed");
        return false;
    }

    if (!connectGPRS()) {
        DEBUG_PRINTLN("ERROR: GPRS connection failed");
        return false;
    }

    DEBUG_PRINTLN("Network connection successful");
    return true;
}

bool ModemManager::waitForNetwork(uint32_t timeout) {
    DEBUG_PRINTLN("Waiting for network registration...");

    if (!modem.waitForNetwork(timeout)) {
        DEBUG_PRINTLN("ERROR: Network registration timeout");
        return false;
    }

    if (modem.isNetworkConnected()) {
        DEBUG_PRINTLN("Network registered");
        return true;
    }

    return false;
}

bool ModemManager::connectGPRS() {
    DEBUG_PRINT("Connecting to APN: ");
    DEBUG_PRINTLN(GPRS_APN);

    if (!modem.gprsConnect(GPRS_APN, GPRS_USER, GPRS_PASS)) {
        DEBUG_PRINTLN("ERROR: GPRS connection failed");
        return false;
    }

    if (modem.isGprsConnected()) {
        DEBUG_PRINTLN("GPRS connected");
        String ip = modem.getLocalIP();
        DEBUG_PRINT("Local IP: ");
        DEBUG_PRINTLN(ip);
        return true;
    }

    return false;
}

bool ModemManager::isConnected() {
    return modem.isGprsConnected();
}

void ModemManager::reconnect() {
    DEBUG_PRINTLN("=== Attempting to reconnect ===");

    // Try to reconnect GPRS
    if (connectGPRS()) {
        DEBUG_PRINTLN("Reconnection successful");
        return;
    }

    // If GPRS failed, try full network reconnect
    DEBUG_PRINTLN("GPRS reconnect failed, trying full network reconnect...");
    if (connectNetwork()) {
        DEBUG_PRINTLN("Full network reconnection successful");
    } else {
        DEBUG_PRINTLN("ERROR: Reconnection failed");
    }
}
