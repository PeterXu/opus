#include "base/common.h"
#include <emscripten.h>
#include <map>
#include "acodec.h"
#include "base/rtputils.h"

namespace audio {

// LocalStream

class LocalStream {
public:
    LocalStream() : m_enc(NULL), m_ptype(-1), m_seq(1000), m_ssrc(0), m_timestamp(0),
    m_frame_size(0), m_delta_ts(0), m_last_time(0)
    {}
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
    void set_input_parameters(int sampleRate, int channels) {
        LOGI("[local] set input parameters: "<<sampleRate<<"/"<<channels);
        if (m_enc) m_enc->set_input_parameters(sampleRate, channels);
    }
    int input(const int16_t *data, int size) {
        if (m_enc) return m_enc->input(data, size);
        return -1;
    }
    int input2(const float *data, int size, int sampleRate, int channels) {
        set_input_parameters(sampleRate, channels);

        // convert from float to int16_t
        Int16Array array;
        array.resize(size);
        for (int i=0; i < size; i++) {
            float s = std::max(-1.0f, std::min(1.0f, data[i]));
            array[i] =  s < 0 ? (s * 0x8000) : (s * 0x7FFF);
        }
        return input(&array[0], size);
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
        int64_t now = NowMs();
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
    Encoder* m_enc;
    int m_ptype;
    uint16_t m_seq;
    uint32_t m_ssrc;
    uint32_t m_timestamp;

    float m_frame_size;
    uint32_t m_delta_ts;
    int64_t m_last_time;
};

// RemoteStream

class RemoteStream {
public:
    RemoteStream() : m_last_dec(NULL)
    {}
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
    void set_output_parameters(int sampleRate, int channels) {
        std::map<int, Decoder*>::iterator it = m_decs.begin();
        for (; it != m_decs.end(); it++) {
            it->second->set_output_parameters(sampleRate, channels);
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
    bool output(Int16Array *out) {
        if (m_last_dec && out) {
            return m_last_dec->output(out);
        } else {
            return false;
        }
    }

private:
    Decoder* m_last_dec;
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
void LocalStream_setInputParameters(audio::LocalStream *self, int sampleRate, int channels)
{
    self->set_input_parameters(sampleRate, channels);
}

EMSCRIPTEN_KEEPALIVE
int LocalStream_input(audio::LocalStream *self, const int16_t *data, int size)
{
    return self->input(data, size);
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
void RemoteStream_setOutputParameters(audio::RemoteStream *self, int sampleRate, int channels)
{
    self->set_output_parameters(sampleRate, channels);
}

EMSCRIPTEN_KEEPALIVE
int RemoteStream_input(audio::RemoteStream *self, const char *data, size_t size)
{
    return self->input(data, size);
}

EMSCRIPTEN_KEEPALIVE
bool RemoteStream_output(audio::RemoteStream *self, Int16Array *out)
{
    return self->output(out);
}

} // extern "C"
