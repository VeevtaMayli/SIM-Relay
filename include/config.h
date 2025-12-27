#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// MODEM CONFIGURATION
// ============================================
// A7670X uses SIM7600 compatible API in TinyGSM
#define TINY_GSM_MODEM_SIM7600

// ============================================
// SERVER CONFIGURATION
// ============================================
#define SERVER_HOST "your-server.com"  // Your backend server hostname or IP
#define SERVER_PORT 80                 // HTTP port (not HTTPS)
#define SERVER_PATH "/api/sms"         // API endpoint path

// ============================================
// GPRS/NETWORK CONFIGURATION
// ============================================
// Magticom (Georgia) - for testing
#define GPRS_APN_MAGTICOM "internet"
#define GPRS_USER_MAGTICOM ""
#define GPRS_PASS_MAGTICOM ""

// MegaFon (Russia) - for production
#define GPRS_APN_MEGAFON "internet"
#define GPRS_USER_MEGAFON ""
#define GPRS_PASS_MEGAFON ""

// Active APN (change based on SIM card)
#define GPRS_APN GPRS_APN_MEGAFON
#define GPRS_USER GPRS_USER_MEGAFON
#define GPRS_PASS GPRS_PASS_MEGAFON

// ============================================
// TIMING CONFIGURATION
// ============================================
#define SMS_CHECK_INTERVAL 10000      // Check for new SMS every 10 seconds
#define NETWORK_CHECK_INTERVAL 60000  // Check network status every 60 seconds
#define HTTP_TIMEOUT 30000            // HTTP request timeout (30 seconds)

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
#define SMS_ENCODING_UCS2 1           // Use UCS2 encoding for SMS (supports Cyrillic, emoji)
#define SMS_DELETE_AFTER_SEND 1       // Delete SMS from SIM after successful send to server

#endif // CONFIG_H
