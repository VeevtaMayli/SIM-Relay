# SIM-Relay

SMS Gateway Device for LilyGO T-Call-A7670X-V1-0

## Overview

SIM-Relay is a firmware for the LilyGO T-Call-A7670X board that monitors incoming SMS messages on a SIM card and forwards them to a backend server via HTTP. The device acts as a "dumb relay" - it receives SMS, performs minimal processing (UCS2 to UTF-8 decoding), and sends the data to your server.

### Architecture

```
SIM Card (SMS + Data)
        ↓
LilyGO T-Call-A7670X (SIM-Relay)
        ↓  HTTP POST (JSON)
Backend API (SIM-Orchestrator)
        ↓
Telegram Bot API (HTTPS)
```

## Features

- **SMS Monitoring**: Automatically checks for new SMS every 10 seconds
- **UCS2/UTF-8 Support**: Decodes Cyrillic text and emoji
- **HTTP POST**: Sends SMS data to backend server as JSON
- **Reliable Delivery**: Only deletes SMS after successful server response (200 OK)
- **Network Recovery**: Automatically reconnects on network failures
- **Configurable**: Easy configuration via `config.h`

## Hardware Requirements

- **Board**: LilyGO T-Call-A7670X-V1-0
- **SIM Card**: Active SIM with SMS and data plan
- **Power**: USB-C or battery

## Supported SIM Cards

### Testing
- **Magticom** (Georgia)
  - APN: `internet`

### Production
- **MegaFon** (Russia)
  - APN: `internet`

## Project Structure

```
SIM-Relay/
├── platformio.ini          # PlatformIO configuration
├── include/
│   ├── config.h            # Configuration file (server, APN, intervals)
│   ├── utilities.h         # Hardware pin definitions
│   ├── modem_manager.h     # Modem initialization and network management
│   ├── sms_reader.h        # SMS reading and UCS2 decoding
│   └── http_sender.h       # HTTP POST to server
├── src/
│   ├── main.cpp            # Main application logic
│   ├── modem_manager.cpp
│   ├── sms_reader.cpp
│   └── http_sender.cpp
└── lib/
    ├── TinyGSM/            # Modem communication library
    ├── ArduinoHttpClient/  # HTTP client library
    └── StreamDebugger/     # AT command debugging
```

## Installation

### 1. Prerequisites

- [PlatformIO Core](https://platformio.org/install) or [PlatformIO IDE](https://platformio.org/platformio-ide)
- USB-C cable for programming

### 2. Clone Repository

```bash
git clone <repository-url>
cd SIM-Relay
```

### 3. Configure

Edit `include/config.h`:

```cpp
// Server configuration
#define SERVER_HOST "your-server.com"  // Your backend server
#define SERVER_PORT 80
#define SERVER_PATH "/api/sms"

// Select APN based on your SIM card
#define GPRS_APN GPRS_APN_MEGAFON  // or GPRS_APN_MAGTICOM

// Optional: Adjust intervals
#define SMS_CHECK_INTERVAL 10000   // Check every 10 seconds
```

### 4. Build and Upload

```bash
# Build firmware
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

## Configuration

### Server Settings

The device sends HTTP POST requests to your backend server:

**Endpoint**: `http://SERVER_HOST:SERVER_PORT/SERVER_PATH`

**Request**:
```http
POST /api/sms HTTP/1.1
Host: your-server.com
Content-Type: application/json

{
  "sender": "+79991234567",
  "text": "Ваш код подтверждения: 1234",
  "timestamp": "2025-12-26 14:30:15"
}
```

**Expected Response**:
```http
HTTP/1.1 200 OK
Content-Type: application/json

{
  "status": "received"
}
```

### APN Configuration

Edit `include/config.h` to match your carrier:

```cpp
// For MegaFon (Russia)
#define GPRS_APN GPRS_APN_MEGAFON

// For Magticom (Georgia)
#define GPRS_APN GPRS_APN_MAGTICOM
```

To add new carriers, add entries to `config.h`:

```cpp
#define GPRS_APN_YOURCARRIER "your.apn.here"
#define GPRS_USER_YOURCARRIER "username"
#define GPRS_PASS_YOURCARRIER "password"
```

### Debug Output

Enable/disable debug messages:

```cpp
#define ENABLE_SERIAL_DEBUG 1  // 1 = enable, 0 = disable
```

## Usage

### Normal Operation

1. Insert SIM card into the board
2. Power on the device
3. Device will:
   - Initialize modem
   - Register on network
   - Connect GPRS
   - Start monitoring SMS

4. When SMS arrives:
   - Device reads and decodes SMS
   - Sends to backend server via HTTP POST
   - If server responds 200 OK → deletes SMS from SIM
   - If error → keeps SMS for retry (10 seconds later)

### Serial Monitor Output

```
========================================
    SIM-Relay: SMS Gateway Device
========================================

Step 1: Initializing modem...
Modem Info: SIMCOM_A7670E_V1.01

Step 2: Connecting to network...
Network registered
GPRS connected
Local IP: 10.123.45.67

Step 3: Initializing SMS reader...
SMS Reader initialized successfully

Step 4: Initializing HTTP sender...
HTTP sender initialized

========================================
   System Ready - Monitoring SMS...
========================================

>>> Found 1 unread SMS <<<

--- Processing SMS 1/1 (Index: 1) ---
From: +79991234567
Text: Ваш код: 1234
=== Sending SMS to Server ===
HTTP Status Code: 200
✓ SMS successfully sent to server
✓ SMS deleted from SIM

--- SMS processing completed ---
```

## Error Handling

### Network Loss

- Device checks network every 60 seconds
- Automatically reconnects if connection lost
- SMS remain on SIM until network restored

### Server Errors

| Error | Device Behavior |
|-------|----------------|
| Connection timeout | SMS stays on SIM, retry in 10s |
| HTTP 5xx | SMS stays on SIM, retry in 10s |
| HTTP 200 OK | SMS deleted from SIM |

### SIM Memory Full

- Maximum ~30-50 SMS on typical SIM
- Device deletes SMS only after successful send
- Monitor serial output for warnings

## Troubleshooting

### Modem won't start

1. Check power supply (USB-C or battery)
2. Check SIM card insertion
3. Try power cycle (unplug/replug)

### Network registration fails

1. Check SIM card has active service
2. Verify APN settings for your carrier
3. Check signal strength (move to window/outdoors)
4. Enable debug and check AT commands

### HTTP POST fails

1. Verify server is reachable
2. Check firewall settings on server
3. Test with `curl` from another device:
   ```bash
   curl -X POST http://your-server.com/api/sms \
     -H "Content-Type: application/json" \
     -d '{"sender":"+test","text":"test","timestamp":"test"}'
   ```

### SMS not decoded properly

- Verify `SMS_ENCODING_UCS2` is enabled in `config.h`
- Check that SMS is in UCS2 format (not GSM7)
- Enable debug output to see raw SMS data

## Development

### Add New Features

1. **Custom SMS Filtering**:
   - Modify `sms_reader.cpp` to filter by sender
   - Add whitelist/blacklist in `config.h`

2. **Additional Data Fields**:
   - Edit `http_sender.cpp` `createJsonPayload()`
   - Add device ID, signal strength, etc.

3. **Local Storage**:
   - Add SD card support for offline buffering
   - Store failed sends for later retry

### Debugging

Enable AT command debugging:

```cpp
// In src/modem_manager.cpp
#define DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
```

## API Contract

### Device → Server

**Endpoint**: `POST /api/sms`

**Content-Type**: `application/json`

**Payload**:
```json
{
  "sender": "string (phone number)",
  "text": "string (UTF-8 decoded message)",
  "timestamp": "string (date/time from SMS)"
}
```

**Success Response**:
```json
HTTP/1.1 200 OK
{
  "status": "received"
}
```

**Error Response**:
```json
HTTP/1.1 500 Internal Server Error
{
  "error": "error description"
}
```

## License

MIT License

## Related Projects

- **SIM-Orchestrator**: Backend server (C# ASP.NET Core) that receives SMS from SIM-Relay and forwards to Telegram
- **LilyGO-Modem-Series**: Original examples and libraries for LilyGO modem boards

## Credits

- Based on [LilyGO T-A76XX](https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX) examples
- UCS2 decoding from ReadSMS example
- TinyGSM library (fork by lewisxhe)

## Support

For issues and questions, please open an issue on GitHub.
