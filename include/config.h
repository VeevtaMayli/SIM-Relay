#ifndef CONFIG_H
#define CONFIG_H

#include "secrets.h"

#define SERVER_PATH "/api/sms"

// ============================================
// TIMING CONFIGURATION
// ============================================
#define SMS_CHECK_INTERVAL 10000      // Check for new SMS every 10 seconds
#define NETWORK_CHECK_INTERVAL 60000  // Check WiFi status every 60 seconds
#define HTTP_TIMEOUT 30000            // HTTP request timeout (30 seconds)
#define WIFI_CONNECT_TIMEOUT 15000    // WiFi connection timeout (15 seconds)

// ============================================
// DEBUG CONFIGURATION
// ============================================
#define ENABLE_SERIAL_DEBUG 1         // 1 = enable debug output, 0 = disable
#define SERIAL_BAUD_RATE 115200       // Serial monitor baud rate

#if ENABLE_SERIAL_DEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(x, ...) Serial.printf(x, __VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x, ...)
#endif

// ============================================
// SMS CONFIGURATION
// ============================================
#define SMS_DELETE_AFTER_SEND 1       // Delete SMS from SIM after successful send to server

#endif // CONFIG_H
