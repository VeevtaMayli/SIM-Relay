#ifndef SMS_CONCATENATOR_H
#define SMS_CONCATENATOR_H

#include <Arduino.h>
#include <map>
#include <vector>
#include "sms_types.h"

// High-level multi-part SMS handler
// Buffers partial messages and concatenates when all parts arrive

struct SmsPartBuffer {
    String sender;
    String timestamp;
    std::vector<String> parts;  // Indexed by partNumber-1
    uint8_t totalParts;
    unsigned long firstPartTime;  // millis() when first part arrived

    SmsPartBuffer() : totalParts(0), firstPartTime(0) {}
};

class SmsConcatenator {
public:
    SmsConcatenator() = default;

    // Add a SMS part and return concatenated message if complete
    // Returns nullptr if more parts are needed
    // For single-part SMS, returns immediately
    SmsMessage* addPart(const SmsMessage& sms);

    // Clean up old partial messages (call periodically)
    void cleanup();

private:
    std::map<uint16_t, SmsPartBuffer> partBuffers;  // Key: refNumber
    static const unsigned long PART_TIMEOUT = 300000; // 5 minutes

    // Static storage for returned result (avoids heap allocation)
    static SmsMessage resultBuffer;
};

#endif // SMS_CONCATENATOR_H
