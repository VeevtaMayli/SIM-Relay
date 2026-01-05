#include "sms/text_decoder.h"

// GSM 7-bit default alphabet table (basic) - UTF-8 strings
const char* const TextDecoder::GSM7_BASIC[128] = {
    "@", "£", "$", "¥", "è", "é", "ù", "ì", "ò", "Ç", "\n", "Ø", "ø", "\r", "Å", "å",
    "Δ", "_", "Φ", "Γ", "Λ", "Ω", "Π", "Ψ", "Σ", "Θ", "Ξ", "\x1B", "Æ", "æ", "ß", "É",
    " ", "!", "\"", "#", "¤", "%", "&", "'", "(", ")", "*", "+", ",", "-", ".", "/",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?",
    "¡", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "Ä", "Ö", "Ñ", "Ü", "§",
    "¿", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
    "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "ä", "ö", "ñ", "ü", "à"
};

// GSM 7-bit extended alphabet (escape sequences, accessed via ESC)
const Gsm7ExtendedChar TextDecoder::GSM7_EXTENDED[10] = {
    {0x0A, "\f"},  // Form feed
    {0x14, "^"},   // Caret
    {0x28, "{"},   // Left brace
    {0x29, "}"},   // Right brace
    {0x2F, "\\"},  // Backslash
    {0x3C, "["},   // Left bracket
    {0x3D, "~"},   // Tilde
    {0x3E, "]"},   // Right bracket
    {0x40, "|"},   // Pipe
    {0x65, "€"}    // Euro sign
};

String TextDecoder::decodeGsm7bit(const uint8_t* data, int charCount, int paddingBits) {
    // GSM 7-bit alphabet unpacking with extended table support
    // Each character is 7 bits, packed into octets
    // paddingBits: bit offset for text after UDH (0-6)

    String result = "";
    result.reserve(charCount);

    int bitOffset = paddingBits;
    bool escapeNext = false;

    for (int i = 0; i < charCount; i++) {
        int byteIndex = bitOffset / 8;
        int bitPos = bitOffset % 8;

        uint8_t char7bit;

        if (bitPos <= 1) {
            // Character fits in one byte
            char7bit = (data[byteIndex] >> bitPos) & Gsm7Const::MASK_7BIT;
        } else {
            // Character spans two bytes
            uint8_t lowBits = data[byteIndex] >> bitPos;
            uint8_t highBits = (byteIndex + 1 < 200) ? (data[byteIndex + 1] << (8 - bitPos)) : 0;
            char7bit = (lowBits | highBits) & Gsm7Const::MASK_7BIT;
        }

        // Handle escape sequences
        if (escapeNext) {
            // Look up in extended table
            bool found = false;
            for (int j = 0; j < 10; j++) {
                if (GSM7_EXTENDED[j].code == char7bit) {
                    result += GSM7_EXTENDED[j].replacement;
                    found = true;
                    break;
                }
            }
            if (!found) {
                result += '?'; // Unknown extended character
            }
            escapeNext = false;
        } else if (char7bit == Gsm7Const::ESCAPE) {
            // Escape character - next char is from extended table
            escapeNext = true;
        } else if (char7bit < 128) {
            result += GSM7_BASIC[char7bit];
        } else {
            result += '?'; // Unknown character
        }

        bitOffset += 7;
    }

    return result;
}

String TextDecoder::decodeUcs2(const uint8_t* data, int byteCount) {
    // UCS-2 to UTF-8 conversion (supports surrogate pairs for emoji)
    String result = "";
    result.reserve(byteCount / 2);

    int i = 0;
    uint16_t prevHigh = 0;

    // Skip BOM (Byte Order Mark) 0xFEFF or 0xFFFE if present
    if (byteCount >= 2) {
        uint16_t firstChar = (data[0] << 8) | data[1];
        if (firstChar == 0xFEFF || firstChar == 0xFFFE) {
            i = 2; // Skip BOM
        }
    }

    while (i < byteCount - 1) {
        uint16_t code = (data[i] << 8) | data[i + 1];
        i += 2;

        if (code >= 0xD800 && code <= 0xDBFF) {
            // High surrogate (first half of emoji)
            prevHigh = code;
            continue;
        } else if (code >= 0xDC00 && code <= 0xDFFF && prevHigh) {
            // Low surrogate - combine with high surrogate
            uint32_t fullCode = 0x10000 + (((prevHigh - 0xD800) << 10) | (code - 0xDC00));

            result += char(0xF0 | (fullCode >> 18));
            result += char(0x80 | ((fullCode >> 12) & 0x3F));
            result += char(0x80 | ((fullCode >> 6) & 0x3F));
            result += char(0x80 | (fullCode & 0x3F));
            prevHigh = 0;
            continue;
        }

        // Regular UCS-2 character
        if (code < 0x80) {
            result += char(code);
        } else if (code < 0x800) {
            result += char(0xC0 | (code >> 6));
            result += char(0x80 | (code & 0x3F));
        } else {
            result += char(0xE0 | (code >> 12));
            result += char(0x80 | ((code >> 6) & 0x3F));
            result += char(0x80 | (code & 0x3F));
        }
    }

    return result;
}

String TextDecoder::decodeUcs2Hex(const String& hexStr) {
    // Legacy hex string decoder (for compatibility)
    if (hexStr.length() % 4 != 0) {
        return "";
    }

    String out = "";
    for (int i = 0; i + 3 < hexStr.length(); i += 4) {
        uint16_t code = (uint16_t)strtol(hexStr.substring(i, i + 4).c_str(), NULL, 16);

        if (code < 0x80) {
            out += char(code);
        } else if (code < 0x800) {
            out += char(0xC0 | (code >> 6));
            out += char(0x80 | (code & 0x3F));
        } else {
            out += char(0xE0 | (code >> 12));
            out += char(0x80 | ((code >> 6) & 0x3F));
            out += char(0x80 | (code & 0x3F));
        }
    }
    return out;
}
