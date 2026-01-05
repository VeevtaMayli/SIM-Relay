#ifndef TEXT_DECODER_H
#define TEXT_DECODER_H

#include <Arduino.h>

// GSM 7-bit constants
namespace Gsm7Const {
    constexpr uint8_t ESCAPE = 0x1B;    // Escape character for extended table
    constexpr uint8_t MASK_7BIT = 0x7F; // Mask for 7-bit character
}

// GSM 7-bit extended character mapping
struct Gsm7ExtendedChar {
    uint8_t code;
    const char* replacement;
};

// Pure functions for text encoding/decoding
class TextDecoder {
public:
    // GSM 7-bit alphabet → UTF-8
    // - data: byte array containing packed 7-bit characters
    // - charCount: number of characters to decode
    // - paddingBits: bit offset for text after UDH (0-6)
    static String decodeGsm7bit(const uint8_t* data, int charCount, int paddingBits = 0);

    // UCS-2 (16-bit Unicode) → UTF-8
    // - data: byte array containing UCS-2 big-endian characters
    // - byteCount: total bytes to decode
    // Supports surrogate pairs for emoji
    static String decodeUcs2(const uint8_t* data, int byteCount);

    // Legacy: UCS-2 hex string → UTF-8 (for compatibility)
    static String decodeUcs2Hex(const String& hexStr);

private:
    // GSM 7-bit alphabet tables (UTF-8 strings for multi-byte chars)
    static const char* const GSM7_BASIC[128];
    static const Gsm7ExtendedChar GSM7_EXTENDED[10];
};

#endif // TEXT_DECODER_H
