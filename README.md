# SIM-Relay

SMS Gateway for LilyGO T-Call-A7670X. Receives SMS via LTE modem, forwards to server via WiFi.

## Architecture

```
SIM Card ──► A7670 Modem ──► SmsManager ───┐
                                           ├──► main.cpp ──► Server (HTTPS)
WiFi Router ──► ESP32 WiFi ──► HttpSender ─┘
```

**Hybrid design**: LTE modem for SMS (PDU mode), ESP32 WiFi for HTTP.
**Features**: Multi-part SMS concatenation, GSM 7-bit & UCS-2 decoding, alphanumeric sender support.

## Quick Start

1. Copy `include/secrets.h.example` to `include/secrets.h`
2. Configure:
   ```cpp
   #define SERVER_HOST "your-server.com"
   #define SERVER_PORT 443
   #define API_KEY "your-secret-api-key-min-32-characters-long"

   #define WIFI_SSID "your-wifi-ssid"
   #define WIFI_PASSWORD "your-wifi-password"
   ```
3. Build and upload:
   ```bash
   pio run --target upload
   pio device monitor
   ```

## Project Structure

```
include/
├── config.h               # Timing, debug settings
├── secrets.h              # Credentials (git-ignored)
├── ca_cert.h              # Let's Encrypt root CA
├── wifi_manager.h         # WiFi connection
├── modem_manager.h        # LTE modem (SMS only)
├── sms_manager.h          # SMS operations (read, delete, list)
├── http_sender.h          # HTTPS POST via WiFi
└── sms/
    ├── sms_types.h        # Data structures
    ├── pdu_parser.h       # PDU → SmsMessage
    ├── text_decoder.h     # GSM7/UCS2 → UTF-8
    └── sms_concatenator.h # Multi-part buffering
```

## API Contract

**Endpoint**: `POST /api/sms`

**Headers**:
```
Content-Type: application/json
X-API-Key: <your-api-key>
```

**Payload**:
```json
{
  "sender": "+79991234567",
  "text": "Your message",
  "timestamp": "2025-12-28 14:30:15"
}
```

**Response**: `200 OK` = SMS deleted from SIM, otherwise retry in 10s.

## Configuration

| Setting | Default | Description |
|---------|---------|-------------|
| `SMS_CHECK_INTERVAL` | 10s | SMS polling interval |
| `NETWORK_CHECK_INTERVAL` | 60s | WiFi check interval |
| `WIFI_CONNECT_TIMEOUT` | 15s | WiFi connection timeout |
| `HTTP_TIMEOUT` | 30s | HTTP request timeout |
| `ENABLE_SERIAL_DEBUG` | 1 | Debug output (0 = off) |

## Troubleshooting

| Problem | Solution |
|---------|----------|
| WiFi won't connect | Check SSID/password in secrets.h |
| Modem not responding | Power cycle, check SIM insertion |
| HTTPS fails | Verify server SSL (Let's Encrypt supported) |
| SMS not decoded | Check debug logs for PDU parsing errors |

## Future: GPRS Fallback

GPRS code is preserved in `modem_manager.cpp` (commented). Uncomment to add WiFi→GPRS fallback.

## License

MIT
