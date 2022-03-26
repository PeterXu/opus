#include "base/common.h"
#include <emscripten.h>
#include <map>
#include "acodec.h"
#include "base/rtputils.h"
#include "base/buffer.h"

namespace audio {

// LocalStream

class LocalStream {
public:
    LocalStream() {}
    ~LocalStream() {
        delete m_enc;
        m_enc = NULL;
    }
    void set_codec_parameters(int codec, float frameSize, int sampleRate, int channels, int bitrate, bool isVoip) {
        if (isCodecValid(codec)) {
            if (m_enc) delete m_enc;
            m_enc = CreateEncoder(codec, frameSize, sampleRate, channels, bitrate, isVoip);
            m_frame_size = frameSize;
            m_delta_ts = sampleRate/1000*frameSize*channels;
            LOGI("[local] set frame-size="<<frameSize<<", delta-ts="<<m_delta_ts);
        } else {
            LOGE("[local] invalid codec="<<codec);
        }
    }
    void set_codec_bitrate(int bitrate) {
        LOGI("[local] set bitrate="<<bitrate);
        if (m_enc) m_enc->set_bitrate(bitrate);
    }
    void set_rtp_parameters(uint32_t ssrc, int payloadType) {
        LOGI("[local] set rtp ssrc="<<ssrc<<", ptype="<<payloadType);
        m_ssrc = ssrc;
        m_ptype = payloadType;
    }
    int input(const int16_t *data, size_t size, int sampleRate, int channels) {
        if (m_enc) return m_enc->input(data, size, sampleRate, channels);
        return -1;
    }
    static inline int16_t clipFloatToInt16(float value) {
        value = (value >= 1.0f) ? 1.0f : ((value <= -1.0f) ? -1.0f : value);
        int16_t s = (value < 0) ? (value * 0x8000) : (value * 0x7FFF);
        return s;
    }
    int input2(const float *data, size_t size, int sampleRate, int channels) {
        if (!(channels == 1 || channels == 2)) {
            return -1;
        }

        // convert float to int16_t
        int16_t* array = m_alloc.get(size); // should not be deleted
        if (channels == 1) {
            for (size_t i=0; i < size; i++) {
                array[i] = clipFloatToInt16(data[i]);
            }
        } else {
            // convert planar to interleaved
            size_t size_per_channel = size / 2;
            for (size_t i=0; i < size_per_channel; i++) {
                array[i*2] = clipFloatToInt16(data[i]);
                array[i*2+1] = clipFloatToInt16(data[i + size_per_channel]);
            }
        }
        return input(array, size, sampleRate, channels);
    }
    bool output(String *out) {
        if (m_enc && out) {
            String frame;
            if (m_enc->output(&frame)) {
                out->resize(frame.size() + 12);
                char *data = (char *)(out->c_str());
                size_t length = out->size();
                cricket::SetRtpHeaderFlags(data, length, false, false, 0);
                cricket::SetRtpPayloadType(data, length, m_ptype);
                cricket::SetRtpPayloadType(data, length, m_seq++);
                cricket::SetRtpTimestamp(data, length, gen_timestamp());
                cricket::SetRtpSsrc(data, length, m_ssrc);
                memcpy(data+12, frame.c_str(), frame.size());
                return true;
            } else {
                LOGE("[local] output nothing");
            }
        } else {
            LOGE("[local] output invalid state");
        }
        return false;
    }

private:
    uint32_t gen_timestamp() {
        uint32_t now = NowMs();
        uint32_t ts = m_timestamp; // last
        if (ts == 0) {
            ts = RandTimestamp();
            LOGI("[local] first timestamp="<<ts);
        } else {
            int delta = 1;
            int timeout = m_frame_size * 250;
            if (m_last_time > 0 && now >= m_last_time + timeout) {
                delta = (now - m_last_time) / m_frame_size;
            }
            ts += (delta * m_delta_ts);
        }
        m_timestamp = ts;
        m_last_time = now;
        return ts;
    }

private:
    Encoder* m_enc = nullptr;
    int m_ptype = -1;
    uint16_t m_seq = 1000;
    uint32_t m_ssrc = 0;
    uint32_t m_timestamp = 0;

    float m_frame_size = 20.0f;
    uint32_t m_delta_ts = 0;
    uint32_t m_last_time = 0;

    MyAlloc<int16_t> m_alloc;
};

// RemoteStream

class RemoteStream {
public:
    RemoteStream() {}
    ~RemoteStream() {
        std::map<int, Decoder*>::iterator it = m_decs.begin();
        for (; it != m_decs.end(); it++) {
            delete it->second;
        }
        m_decs.clear();
    }
    void register_payload_type(int payloadType, int codec, int sampleRate, int channels) {
        if (isCodecValid(codec)) {
            Decoder* dec = m_decs[payloadType];
            if (dec == m_last_dec) m_last_dec = NULL;
            if (dec) delete dec;
            m_decs[payloadType] = CreateDecoder(codec, sampleRate, channels);
        }
    }
    int input(const char *data, size_t size) {
        int iret = -1;
        if (cricket::IsRtpPacket(data, size)) {
            size_t headLen = 0;
            if (cricket::GetRtpHeaderLen(data, size, &headLen)) {
                int ptype = 0;
                cricket::GetRtpPayloadType(data, size, &ptype);
                Decoder* dec = m_decs[ptype];
                if (dec) {
                    iret = dec->input(data + headLen, size - headLen);
                    m_last_dec = dec;
                }
            }
        }
        return iret;
    }
    bool output(Int16Array *out, int sampleRate, int channels) {
        if (m_last_dec && out) {
            return m_last_dec->output(out, sampleRate, channels);
        } else {
            return false;
        }
    }
    static inline float clipInt16ToFloat(int16_t value) {
        float s = (value < 0) ? (value * 1.0f / 0x8000) : (value * 1.0f / 0x7FFF);
        s = (s >= 1.0f) ? 1.0f : ((s <= -1.0f) ? -1.0f : s);
        return s;
    }
    bool output2(Float32Array *out, int sampleRate, int channels) {
        if (!m_last_dec || !out) {
            return false;
        }

        int outputSampleRate = sampleRate;
        int outputChannels = channels;
        if (!m_last_dec->check_output_paramters(&outputSampleRate, &outputChannels)) {
            LOGE("[remote] output2 invalid parameters="<<sampleRate<<"/"<<channels);
            return false;
        }

        Int16Array tmpOut;
        if (!m_last_dec->output(&tmpOut, sampleRate, channels)) {
            return false;
        }

        const int16_t *data = &tmpOut[0];
        size_t size = tmpOut.size();

        // convert int16_t to float
        out->resize(size);
        auto &array = *out;
        if (outputChannels == 1) {
            for (size_t i=0; i < size; i++) {
                array[i] = clipInt16ToFloat(data[i]);
            }
        } else {
            // convert interleaved to planar
            size_t size_per_channel = size / outputChannels;
            for (size_t i=0; i < size_per_channel; i++) {
                array[i] = clipInt16ToFloat(data[i*2]);
                array[i+size_per_channel] = clipInt16ToFloat(data[i*2+1]);
            }
        }
        return true;
    }

private:
    Decoder* m_last_dec = nullptr;
    std::map<int, Decoder*> m_decs;
};

} // namespace audio



extern "C" {

// LocalStream

EMSCRIPTEN_KEEPALIVE
audio::LocalStream* LocalStream_new()
{
    return new audio::LocalStream();
}

EMSCRIPTEN_KEEPALIVE
void LocalStream_delete(audio::LocalStream *self)
{
    delete self;
}

EMSCRIPTEN_KEEPALIVE
void LocalStream_setCodecParameters(audio::LocalStream *self,
        int codec, float frameSize, int sampleRate, int channels, int bitrate, bool isVoip)
{
    self->set_codec_parameters(codec, frameSize, sampleRate, channels, bitrate, isVoip);
}

EMSCRIPTEN_KEEPALIVE
void LocalStream_setCodecBitrate(audio::LocalStream *self, int bitrate)
{
    self->set_codec_bitrate(bitrate);
}

EMSCRIPTEN_KEEPALIVE
void LocalStream_setRtpParameters(audio::LocalStream *self, int ssrc, int payloadType)
{
    self->set_rtp_parameters((uint32_t)ssrc, payloadType);
}

EMSCRIPTEN_KEEPALIVE
int LocalStream_input(audio::LocalStream *self, const int16_t *data, int size, int sampleRate, int channels)
{
    return self->input(data, size, sampleRate, channels);
}

EMSCRIPTEN_KEEPALIVE
int LocalStream_input2(audio::LocalStream *self, const float *data, int size, int sampleRate, int channels)
{
    return self->input2(data, size, sampleRate, channels);
}

EMSCRIPTEN_KEEPALIVE
bool LocalStream_output(audio::LocalStream *self, String *out)
{
    return self->output(out);
}

// RemoteStream

EMSCRIPTEN_KEEPALIVE
audio::RemoteStream* RemoteStream_new()
{
    return new audio::RemoteStream();
}

EMSCRIPTEN_KEEPALIVE
void RemoteStream_delete(audio::RemoteStream *self)
{
    delete self;
}

EMSCRIPTEN_KEEPALIVE
void RemoteStream_registerPayloadType(audio::RemoteStream *self,
        int payloadType, int codec, int sampleRate, int channels)
{
    self->register_payload_type(payloadType, codec, sampleRate, channels);
}

EMSCRIPTEN_KEEPALIVE
int RemoteStream_input(audio::RemoteStream *self, const char *data, int size)
{
    return self->input(data, size);
}

EMSCRIPTEN_KEEPALIVE
bool RemoteStream_output(audio::RemoteStream *self, Int16Array *out, int sampleRate, int channels)
{
    return self->output(out, sampleRate, channels);
}

EMSCRIPTEN_KEEPALIVE
bool RemoteStream_output2(audio::RemoteStream *self, Float32Array *out, int sampleRate, int channels)
{
    return self->output2(out, sampleRate, channels);
}


} // extern "C"
