#ifndef _BASE_RTPUTILS_H_
#define _BASE_RTPUTILS_H_

#include <stdint.h>
#include <stdlib.h>

namespace cricket {

const size_t kMinRtpPacketLen = 12;
const size_t kMaxRtpPacketLen = 2048;
const size_t kMinRtcpPacketLen = 4;

enum RtcpTypes {
    kRtcpTypeSR = 200,      // Sender report payload type.
    kRtcpTypeRR = 201,      // Receiver report payload type.
    kRtcpTypeSDES = 202,    // SDES payload type.
    kRtcpTypeBye = 203,     // BYE payload type.
    kRtcpTypeApp = 204,     // APP payload type.
    kRtcpTypeRTPFB = 205,   // Transport layer Feedback message payload type.
    kRtcpTypePSFB = 206,    // Payload-specific Feedback message payload type.
};

bool GetRtpFlags(const void* data, size_t len, int* value);
bool GetRtpPayloadType(const void* data, size_t len, int* value);
bool GetRtpSeqNum(const void* data, size_t len, int* value);
bool GetRtpTimestamp(const void* data, size_t len, uint32_t* value);
bool GetRtpSsrc(const void* data, size_t len, uint32_t* value);
bool GetRtpHeaderLen(const void* data, size_t len, size_t* value);

bool GetRtcpType(const void* data, size_t len, int* value);
bool GetRtcpSsrc(const void* data, size_t len, uint32_t* value);

bool SetRtpPayloadType(void* data, size_t len, int value);
bool SetRtpSeqNum(void* data, size_t len, int value);
bool SetRtpTimestamp(void* data, size_t len, uint32_t value);
bool SetRtpSsrc(void* data, size_t len, uint32_t value);
bool SetRtpHeaderFlags(void* data, size_t len, bool padding, bool extension, int csrc_count);


bool IsRtcpPacket(const void* data, size_t len);
bool IsRtpPacket(const void* data, size_t len);
bool IsRtpRtcpPacket(const void* data, size_t len);

} // namespace cricket

#endif // _BASE_RTPUTILS_H_
