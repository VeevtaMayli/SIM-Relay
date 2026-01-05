#include "sms/sms_concatenator.h"
#include "config.h"

// Static storage for result (avoids heap allocation on each call)
SmsMessage SmsConcatenator::resultBuffer;

SmsMessage* SmsConcatenator::addPart(const SmsMessage& sms) {
    if (!sms.partInfo.isMultiPart) {
        // Single-part SMS, return as-is
        resultBuffer = sms;
        return &resultBuffer;
    }

    uint16_t ref = sms.partInfo.refNumber;

    // Initialize buffer if this is the first part
    if (partBuffers.find(ref) == partBuffers.end()) {
        partBuffers[ref].sender = sms.sender;
        partBuffers[ref].timestamp = sms.timestamp;
        partBuffers[ref].totalParts = sms.partInfo.totalParts;
        partBuffers[ref].parts.resize(sms.partInfo.totalParts);
        partBuffers[ref].firstPartTime = millis();
    }

    SmsPartBuffer& buffer = partBuffers[ref];

    // Store this part (1-based indexing)
    int idx = sms.partInfo.partNumber - 1;
    if (idx >= 0 && idx < buffer.totalParts) {
        buffer.parts[idx] = sms.text;
    }

    // Check if all parts received
    bool allReceived = true;
    for (int i = 0; i < buffer.totalParts; i++) {
        if (buffer.parts[i].length() == 0) {
            allReceived = false;
            break;
        }
    }

    if (allReceived) {
        // Concatenate all parts
        resultBuffer.index = sms.index;  // Use last part's index
        resultBuffer.sender = buffer.sender;
        resultBuffer.timestamp = buffer.timestamp;
        resultBuffer.text = "";
        for (int i = 0; i < buffer.totalParts; i++) {
            resultBuffer.text += buffer.parts[i];
        }
        resultBuffer.partInfo.isMultiPart = false;  // Mark as complete

        // Remove from buffer
        partBuffers.erase(ref);

        DEBUG_PRINTF("✓ Concatenated %d-part SMS (ref: %d)\n", buffer.totalParts, ref);
        return &resultBuffer;
    }

    return nullptr;  // More parts needed
}

void SmsConcatenator::cleanup() {
    unsigned long now = millis();
    for (auto it = partBuffers.begin(); it != partBuffers.end(); ) {
        if (now - it->second.firstPartTime > PART_TIMEOUT) {
            DEBUG_PRINTF("⚠ Timeout: Dropping incomplete multi-part SMS (ref: %d)\n", it->first);
            it = partBuffers.erase(it);
        } else {
            ++it;
        }
    }
}
