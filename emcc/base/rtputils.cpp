#include "base/rtputils.h"

namespace cricket {

inline void Set8(void* memory, size_t offset, uint8_t v) {
    static_cast<uint8_t*>(memory)[offset] = v;
}
inline uint8_t Get8(const void* memory, size_t offset) {
    return static_cast<const uint8_t*>(memory)[offset];
}
inline void SetBE16(void* memory, uint16_t v) {
    Set8(memory, 0, static_cast<uint8_t>(v >> 8));
    Set8(memory, 1, static_cast<uint8_t>(v >> 0));
}
inline void SetBE32(void* memory, uint32_t v) {
    Set8(memory, 0, static_cast<uint8_t>(v >> 24));
    Set8(memory, 1, static_cast<uint8_t>(v >> 16));
    Set8(memory, 2, static_cast<uint8_t>(v >> 8));
    Set8(memory, 3, static_cast<uint8_t>(v >> 0));
}
inline uint16_t GetBE16(const void* memory) {
    return static_cast<uint16_t>((Get8(memory, 0) << 8) | (Get8(memory, 1) << 0));
}
inline uint32_t GetBE32(const void* memory) {
    return (static_cast<uint32_t>(Get8(memory, 0)) << 24) |
        (static_cast<uint32_t>(Get8(memory, 1)) << 16) |
        (static_cast<uint32_t>(Get8(memory, 2)) << 8) |
        (static_cast<uint32_t>(Get8(memory, 3)) << 0);
}


bool GetUint8(const void* data, size_t offset, int* value) {
    if (!data || !value) {
        return false;
    }
    *value = *(static_cast<const uint8_t*>(data) + offset);
    return true;
}

bool GetUint16(const void* data, size_t offset, int* value) {
    if (!data || !value) {
        return false;
    }
    *value = static_cast<int>(
            GetBE16(static_cast<const uint8_t*>(data) + offset));
    return true;
}

bool GetUint32(const void* data, size_t offset, uint32_t* value) {
    if (!data || !value) {
        return false;
    }
    *value = GetBE32(static_cast<const uint8_t*>(data) + offset);
    return true;
}

bool SetUint8(void* data, size_t offset, int value) {
    if (!data) {
        return false;
    }
    Set8(data, offset, value);
    return true;
}

bool SetUint16(void* data, size_t offset, int value) {
    if (!data) {
        return false;
    }
    SetBE16(static_cast<uint8_t*>(data) + offset, value);
    return true;
}

bool SetUint32(void* data, size_t offset, uint32_t value) {
    if (!data) {
        return false;
    }
    SetBE32(static_cast<uint8_t*>(data) + offset, value);
    return true;
}


/// rtp utils

static const int kRtpVersion = 2;
static const size_t kRtpFlagsOffset = 0;
static const size_t kRtpPayloadTypeOffset = 1;
static const size_t kRtpSeqNumOffset = 2;
static const size_t kRtpTimestampOffset = 4;
static const size_t kRtpSsrcOffset = 8;
static const size_t kRtcpPayloadTypeOffset = 1;

bool GetRtpFlags(const void* data, size_t len, int* value) {
    if (len < kMinRtpPacketLen) {
        return false;
    }
    return GetUint8(data, kRtpFlagsOffset, value);
}

bool GetRtpPayloadType(const void* data, size_t len, int* value) {
    if (len < kMinRtpPacketLen) {
        return false;
    }
    if (!GetUint8(data, kRtpPayloadTypeOffset, value)) {
        return false;
    }
    *value &= 0x7F;
    return true;
}

bool GetRtpSeqNum(const void* data, size_t len, int* value) {
    if (len < kMinRtpPacketLen) {
        return false;
    }
    return GetUint16(data, kRtpSeqNumOffset, value);
}

bool GetRtpTimestamp(const void* data, size_t len, uint32_t* value) {
    if (len < kMinRtpPacketLen) {
        return false;
    }
    return GetUint32(data, kRtpTimestampOffset, value);
}

bool GetRtpSsrc(const void* data, size_t len, uint32_t* value) {
    if (len < kMinRtpPacketLen) {
        return false;
    }
    return GetUint32(data, kRtpSsrcOffset, value);
}

bool GetRtpHeaderLen(const void* data, size_t len, size_t* value) {
    if (!data || len < kMinRtpPacketLen || !value) return false;
    const uint8_t* header = static_cast<const uint8_t*>(data);
    // Get base header size + length of CSRCs (not counting extension yet).
    size_t header_size = kMinRtpPacketLen + (header[0] & 0xF) * sizeof(uint32_t);
    if (len < header_size) return false;
    // If there's an extension, read and add in the extension size.
    if (header[0] & 0x10) {
        if (len < header_size + sizeof(uint32_t)) return false;
        header_size += ((GetBE16(header + header_size + 2) + 1) *
                sizeof(uint32_t));
        if (len < header_size) return false;
    }
    *value = header_size;
    return true;
}

bool GetRtcpType(const void* data, size_t len, int* value) {
    if (len < kMinRtcpPacketLen) {
        return false;
    }
    return GetUint8(data, kRtcpPayloadTypeOffset, value);
}

// This method returns SSRC first of RTCP packet, except if packet is SDES.
// TODO(mallinath) - Fully implement RFC 5506. This standard doesn't restrict
// to send non-compound packets only to feedback messages.
bool GetRtcpSsrc(const void* data, size_t len, uint32_t* value) {
    // Packet should be at least of 8 bytes, to get SSRC from a RTCP packet.
    if (!data || len < kMinRtcpPacketLen + 4 || !value) return false;
    int pl_type;
    if (!GetRtcpType(data, len, &pl_type)) return false;
    // SDES packet parsing is not supported.
    if (pl_type == kRtcpTypeSDES) return false;
    *value = GetBE32(static_cast<const uint8_t*>(data) + 4);
    return true;
}


// Assumes marker bit is 0.
bool SetRtpPayloadType(void* data, size_t len, int value) {
    if (value > 0x7F) {
        return false;
    }
    // now it supports marker bit
    int old_value = 0;
    if (!GetUint8(data, kRtpPayloadTypeOffset, &old_value)) {
        return false;
    }
    int new_value = (old_value & 0x80) | (value & 0x7F);
    return SetUint8(data, kRtpPayloadTypeOffset, new_value);
}

bool SetRtpSeqNum(void* data, size_t len, int value) {
    return SetUint16(data, kRtpSeqNumOffset, value);
}

bool SetRtpTimestamp(void* data, size_t len, uint32_t value) {
    return SetUint32(data, kRtpTimestampOffset, value);
}

bool SetRtpSsrc(void* data, size_t len, uint32_t value) {
    return SetUint32(data, kRtpSsrcOffset, value);
}

bool SetRtpHeaderFlags(void* data, size_t len, bool padding, bool extension, int csrc_count) {
    if (csrc_count > 0x0F) {
        return false;
    }
    int flags = 0;
    flags |= (kRtpVersion << 6);
    flags |= ((padding ? 1 : 0) << 5);
    flags |= ((extension ? 1 : 0) << 4);
    flags |= csrc_count;
    return SetUint8(data, kRtpFlagsOffset, flags);
}

bool IsRtcpPacket(const void* data, size_t len) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    int type = (len >= kMinRtcpPacketLen) ? (ptr[1] & 0x7F) : 0;
    return ((ptr[0] & 0xC0) == 0x80 && type >= 64 && type < 96);
}

bool IsRtpPacket(const void* data, size_t len) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    return (len >= kMinRtpPacketLen && (ptr[0] & 0xC0) == 0x80);
}

bool IsRtpRtcpPacket(const void* data, size_t len) {
	return IsRtpPacket(data, len) || IsRtcpPacket(data, len);
}

} // namespace cricket
