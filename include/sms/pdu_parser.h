#ifndef PDU_PARSER_H
#define PDU_PARSER_H

#include <Arduino.h>
#include "sms_types.h"

// PDU constants
namespace PduConst {
    // PDU Type flags
    constexpr uint8_t UDHI_FLAG = 0x40;  // Bit 6: User Data Header Indicator

    // Type of Address
    constexpr uint8_t TOA_TYPE_MASK = 0x70;      // Bits 6-4: address type
    constexpr uint8_t TOA_ALPHANUMERIC = 0x50;   // 101 = alphanumeric sender

    // Data Coding Scheme (DCS)
    constexpr uint8_t DCS_ENCODING_MASK = 0x0C;  // Bits 3-2: encoding type
    constexpr uint8_t DCS_GSM7 = 0x00;           // GSM 7-bit alphabet
    constexpr uint8_t DCS_8BIT = 0x04;           // 8-bit data
    constexpr uint8_t DCS_UCS2 = 0x08;           // UCS-2 (16-bit Unicode)

    // User Data Header IEI (Information Element Identifier)
    constexpr uint8_t IEI_CONCAT_8BIT = 0x00;    // Concatenated SMS (8-bit reference)
    constexpr uint8_t IEI_CONCAT_16BIT = 0x08;   // Concatenated SMS (16-bit reference)

    // Bit masks
    constexpr uint8_t NIBBLE_LOW = 0x0F;         // Lower 4 bits
    constexpr uint8_t NIBBLE_HIGH = 0xF0;        // Upper 4 bits
}

// Low-level PDU parsing
// Converts raw PDU hex string → SmsMessage structure
class PduParser {
public:
    // Parse PDU hex string into SmsMessage
    // Returns true on success, false on parse error
    static bool parse(const String& pduHex, SmsMessage& out);

private:
    // Helper: hex char pair → byte
    static uint8_t hexToByte(char high, char low);

    // Decode sender address (phone number or alphanumeric)
    static String decodeSender(const String& pdu, int& pos, int senderLen, uint8_t typeOfAddr);
    static String decodePhoneNumber(const String& pdu, int& pos, int digitCount);
    static String decodeAlphanumeric(const String& pdu, int& pos, int senderLen);

    // Decode timestamp (7 octets, semi-octet format)
    static String decodeTimestamp(const String& pdu, int& pos);

    // Parse UDH (User Data Header) for multi-part SMS
    static bool parseUdh(const uint8_t* data, int udhLen, SmsPartInfo& partInfo);

    // Decode user data (text) based on DCS encoding
    static String decodeUserData(const String& pdu, int& pos, uint8_t dcs, int udl,
                                  bool hasUdh, SmsPartInfo& partInfo);
};

#endif // PDU_PARSER_H
