#include "acodec.h"
#include <emscripten.h>

#include "opus.h"
#include "g711/g711_interface.h"
#include "base/signal_processing.h"

namespace audio {

///
/// audio encoder
///

class EmptyEncoder : public Encoder {
public:
    virtual ~EmptyEncoder() {}
    virtual void set_complexity(int complexity) {}
    virtual void set_bitrate(int bitrate) {}
    virtual void set_input_parameters(int sampleRate, int channels) {}
    virtual int input(const int16_t* data, int size) {return -1;}
    virtual bool output(String *out) {return false;}
};

class BaseEncoder : public Encoder {
public:
    static const int kMaxDataSizeSamples = 3840;
    static constexpr int kNativeSampleRatesHz[] = {
        8000, 16000, 32000, 48000
    };
    static const size_t kNumNativeSampleRates =
        sizeof(kNativeSampleRatesHz)/sizeof(kNativeSampleRatesHz[0]);

public:
    BaseEncoder(int codec, int sampleRate, int channels, int bitrate, float frameSize)
        : m_codec(codec),
        m_sampleRate(sampleRate), m_channels(channels), m_bitrate(bitrate), m_frameSize(frameSize),
        m_input_mode(0), m_resampler(NULL)
    {}
    virtual ~BaseEncoder() {
        delete m_resampler;
        m_resampler = NULL;
        m_samples_list.clear();
    }

    // base interfaces
    virtual void set_complexity(int complexity) {}
    virtual void set_bitrate(int bitrate) {}
    virtual void set_input_parameters(int sampleRate, int channels) {
        LOGI("[enc] input parameters="<<sampleRate<<"/"<<channels);
        if (m_resampler == NULL) {
            m_resampler = new MyResampler(sampleRate, channels, m_sampleRate, m_channels);
        } else {
            m_resampler->Reset(sampleRate, channels, m_sampleRate, m_channels);
        }
    }
    virtual void set_input_mode(int mode) {
        // mode: 0 - frame by frame, else continuous samples.
        if (mode != m_input_mode) {
            m_input_mode = mode;
            m_samples_list.clear(); // mode-0
            m_samples.clear();      // non mode-0
        }
    }
    int check_samples(int length) {
        int iret = 0;
        if (m_input_mode == 0) {
            while (m_samples_list.size() >= MAX_SAMPLES_NUMBER) {
                m_samples_list.pop_front();
            }
            while(!m_samples_list.empty()) {
                auto it = m_samples_list.front();
                if (it.size() < length) {
                    m_samples_list.pop_front();
                } else {
                    iret = 1;
                    break;
                }
            }
        } else {
            if (m_samples.size() >= MAX_SAMPLES_NUMBER * 960) {
                // TODO
            }
            if (m_samples.size() >= length) {
                iret = 1;
            }
        }
        return iret;
    }
    void push_samples(const int16_t* data, int size) {
        if (m_input_mode == 0) {
            Int16Array samples(data, data + size);
            m_samples_list.push_back(samples);
        } else {
            m_samples.insert(m_samples.end(), data, data+size);
        }
    }
    bool pop_samples(Int16Array &samples, size_t length) {
        if (m_input_mode == 0) {
            if (!m_samples_list.empty()) {
                auto it = m_samples_list.front();
                if (it.size() >= length) {
                    samples.assign(it.begin(), it.end());
                }
                m_samples_list.pop_front();
            }
        } else {
            if (m_samples.size() >= length) {
                auto it = m_samples.begin();
                samples.assign(it, it + length);
                m_samples.erase(it, it + length);
            }
        }
        return !samples.empty();
    }
    virtual int input(const int16_t* data, int size) {
        if (m_resampler) {
            int iret = m_resampler->Push(data, (size_t)size);
            LOGI("[enc] input resample size="<<size<<" to "<<iret);
            if (iret > 0) {
                const int16_t* newData = m_resampler->Data();
                push_samples(newData, iret);
            } else if (iret == 0) {
                push_samples(data, size);
            } else {
                LOGE("encode-input resampler failed");
                return -1;
            }
        } else {
            push_samples(data, size);
        }
        size_t sampleSize = getRawSampleSize();
        return check_samples(sampleSize);
    }
    virtual bool output(String *out) {
        if (!out) {
            return false;
        }

        Int16Array samples;
        size_t sampleSize = getRawSampleSize();
        if (!pop_samples(samples, sampleSize)) {
            return false;
        }

        size_t bufferSize = getMaxEncodedSize();
        uint8_t *buffer = new uint8_t[bufferSize];
        size_t iret = encodePacket(samples, sampleSize, buffer, bufferSize);
        if (iret > 0) {
            out->assign((char *)buffer, iret);
        }

        delete [] buffer;
        return (iret > 0);
    }

protected:
    virtual size_t getRawSampleSize() {
        return static_cast<size_t>(m_sampleRate/1000.0*m_frameSize*m_channels);
    }
    virtual size_t getMaxEncodedSize() {
        return getRawSampleSize() * 8;
    }
    virtual size_t encodePacket(Int16Array &samples, int sampleSize, uint8_t *buffer, size_t bufferSize) = 0;

protected:
    int m_codec;
    int m_sampleRate; // HZ
    int m_channels;
    int m_bitrate; // bps
    float m_frameSize; //ms

private:
    int m_input_mode;
    MyResampler *m_resampler;
    Int16Array m_samples;
    std::deque<Int16Array> m_samples_list;
};

class CG711Encoder : public BaseEncoder {
public:
    CG711Encoder(int codec, float frameSize) : BaseEncoder(codec, 8000, 1, 0, frameSize)
    {}
    virtual ~CG711Encoder() {}
    virtual size_t encodePacket(Int16Array &samples, int sampleSize, uint8_t *buffer, size_t bufferSize) {
        size_t iret = 0;
        if (m_codec == PCMA) {
            iret = WebRtcG711_EncodeA(&(samples[0]), sampleSize, buffer);
        } else {
            iret = WebRtcG711_EncodeU(&(samples[0]), sampleSize, buffer);
        }
        LOGI("yzxu, total-size="<<samples.size()<<",used="<<sampleSize<<",max="<<bufferSize<<", ret="<<iret);
        return iret;
    }
};

class COpusEncoder : public BaseEncoder {
public:
    COpusEncoder(float frameSize, int sampleRate, int channels, int bitrate, bool isVoip)
        : BaseEncoder(OPUS, sampleRate, channels, bitrate, frameSize), m_enc(NULL) {
        int err = 0;
        int application = (isVoip ? OPUS_APPLICATION_VOIP : OPUS_APPLICATION_AUDIO);
        m_enc = opus_encoder_create(sampleRate, channels, application, &err);
        if (m_enc != NULL){
            if (bitrate <= 0) {
                opus_encoder_ctl(m_enc, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));
            } else {
                opus_encoder_ctl(m_enc, OPUS_SET_BITRATE(bitrate));
            }
        } else {
            LOGE("[opus-enc] error while creating opus encoder: "<<err);
        }
    }
    virtual ~COpusEncoder() {
        if (m_enc) {
            opus_encoder_destroy(m_enc);
            m_enc = NULL;
        }
    }
    virtual void set_complexity(int complexity) {
        if (m_enc != NULL) {
            opus_encoder_ctl(m_enc, OPUS_SET_COMPLEXITY(complexity));
        }
    }
    virtual void set_bitrate(int bitrate) {
        if (m_enc != NULL){
            if (bitrate <= 0) {
                opus_encoder_ctl(m_enc, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));
            } else {
                opus_encoder_ctl(m_enc, OPUS_SET_BITRATE(bitrate));
            }
        }
    }
    virtual size_t getMaxEncodedSize() {
        // Calculate the number of bytes we expect the encoder to produce,
        // then multiply by two to give a wide margin for error.
        const size_t bytes_per_ms = static_cast<size_t>(m_bitrate/(1000*8) + 1);
        const size_t num_10ms_frames_per_packet = static_cast<size_t>(m_frameSize / 10);
        const size_t approx_encoded_bytes = num_10ms_frames_per_packet * 10 * bytes_per_ms;
        return 2 * approx_encoded_bytes;
    }
    virtual size_t encodePacket(Int16Array &samples, int sampleSize, uint8_t *buffer, size_t bufferSize) {
        size_t iret = 0;
        if (m_enc) {
            iret = opus_encode(m_enc, &samples[0], sampleSize/m_channels, buffer, bufferSize);
        }
        return iret;
    }

private:
    OpusEncoder *m_enc;
};

Encoder* CreateEncoder(int codec, float frameSize, int sampleRate, int channels, int bitrate, bool isVoip) {
    if (codec == OPUS) {
        return new COpusEncoder(frameSize, sampleRate, channels, bitrate, isVoip);
    } else if (codec == PCMA || codec == PCMU) {
        return new CG711Encoder(codec, frameSize);
    } else {
        return new EmptyEncoder();
    }
}

///
/// audio decoder
///

class EmptyDecoder : public Decoder {
public:
    virtual ~EmptyDecoder() {}
    virtual void set_output_parameters(int sampleRate, int channels) {}
    virtual int input(const char *data, size_t size) {return -1;}
    virtual bool output(Int16Array *out) {return false;}
};

class BaseDecoder : public Decoder {
public:
    BaseDecoder(int codec, int sampleRate, int channels)
        : m_codec(codec), m_sampleRate(sampleRate), m_channels(channels), m_resampler(NULL)
    {}
    virtual ~BaseDecoder() {
        delete m_resampler;
        m_resampler = NULL;
        m_packet_list.clear();
    }
    virtual void set_output_parameters(int sampleRate, int channels) {
        LOGI("[dec] output parameters="<<sampleRate<<"/"<<channels);
        if (m_resampler == NULL) {
            m_resampler = new MyResampler(m_sampleRate, m_channels, sampleRate, channels);
        } else {
            m_resampler->Reset(m_sampleRate, m_channels, sampleRate, channels);
        }
    }
    virtual int input(const char *data, size_t size) {
        if (m_packet_list.size() >= MAX_PACKET_NUMBER) {
            m_packet_list.pop_front();
        }
        Uint8Array packet(data, data+size);
        m_packet_list.push_back(packet);
        return 1;
    }
    virtual bool output(Int16Array *out) {
        if (!out || m_packet_list.empty()) {
            return false;
        }

        size_t bufferSize = getMaxDecodedSize();
        int16_t *buffer = new int16_t[bufferSize];
        std::deque<Uint8Array>::reference packet = m_packet_list.front();

        size_t iret = decodePacket(packet, buffer, bufferSize);
        if (iret > 0) {
            if (m_resampler) {
                int iret2 = m_resampler->Push(buffer, iret);
                if (iret2 > 0) {
                    const int16_t* newData = m_resampler->Data();
                    *out = Int16Array(newData, newData + iret2);
                    iret = iret2;
                } else if (iret2 == 0) {
                    *out = Int16Array(buffer, buffer + iret);
                } else {
                    LOGE("decode-output resampler failed");
                    iret = -1;
                }
            } else {
                *out = Int16Array(buffer, buffer + iret);
            }
        }

        m_packet_list.pop_front();
        delete []buffer;
        return (iret > 0);
    }

protected:
    virtual size_t getMaxDecodedSize() {
        return m_sampleRate/1000.0*120*m_channels; // using 120ms max.
    }
    virtual size_t decodePacket(Uint8Array &packet, int16_t *buffer, size_t buffer_size) = 0;

protected:
    int m_codec;
    int m_sampleRate;
    int m_channels;

private:
    std::deque<Uint8Array> m_packet_list;
    MyResampler *m_resampler;
};

class CG711Decoder : public BaseDecoder {
public:
    CG711Decoder(int codec) : BaseDecoder(codec, 8000, 1) {}
    virtual ~CG711Decoder() {}
    virtual size_t decodePacket(Uint8Array &packet, int16_t *buffer, size_t bufferSize) {
        int16_t iret = 0;
        int16_t speechType = 0;
        if (m_codec == PCMA) {
            iret = WebRtcG711_DecodeA(&packet[0], packet.size(), buffer, &speechType);
        } else {
            iret = WebRtcG711_DecodeU(&packet[0], packet.size(), buffer, &speechType);
        }
        return size_t(iret);
    }
};

class COpusDecoder : public BaseDecoder {
public:
    COpusDecoder(int sampleRate, int channels)
        : BaseDecoder(OPUS, sampleRate, channels), m_dec(NULL) {
        int err = 0;
        m_dec = opus_decoder_create(sampleRate, channels, &err);
        if (m_dec == NULL) {
            LOGE("[opus-dec] error while creating opus decoder: "<<err);
        }
    }
    virtual ~COpusDecoder() {
        if (m_dec) {
            opus_decoder_destroy(m_dec);
            m_dec = NULL;
        }
    }
    virtual size_t decodePacket(Uint8Array &packet, int16_t *buffer, size_t bufferSize) {
        int iret = 0;
        if (m_dec) {
            iret = opus_decode(m_dec, &packet[0], packet.size(), buffer, bufferSize/m_channels, 0);
        }
        return size_t(iret);
    }

private:
    OpusDecoder *m_dec;
};

Decoder* CreateDecoder(int codec, int sampleRate, int channels) {
    if (codec == OPUS) {
        return new COpusDecoder(sampleRate, channels);
    } else if (codec == PCMA || codec == PCMU) {
        return new CG711Decoder(codec);
    } else {
        return new EmptyDecoder();
    }
}

} // namespace audio



extern "C" {

// Encoder

EMSCRIPTEN_KEEPALIVE
audio::Encoder* Encoder_new(int codec, float frameSize, int sampleRate, int channels, int bitrate, bool isVoip)
{
    LOGI("Encoder_new, "<<getCodecName(codec)<<","<<frameSize<<"ms,"<<sampleRate<<"/"<<channels<<","<<bitrate<<"bps,"<<isVoip);
    return audio::CreateEncoder(codec, frameSize, sampleRate, channels, bitrate, isVoip);
}

EMSCRIPTEN_KEEPALIVE
void Encoder_delete(audio::Encoder *self)
{
    delete self;
}

EMSCRIPTEN_KEEPALIVE
void Encoder_setComplexity(audio::Encoder *self, int complexity)
{
    self->set_complexity(complexity);
}

EMSCRIPTEN_KEEPALIVE
void Encoder_setBitrate(audio::Encoder *self, int bitrate)
{
    self->set_bitrate(bitrate);
}

EMSCRIPTEN_KEEPALIVE
void Encoder_setInputParameters(audio::Encoder *self, int sampleRate, int channels)
{
    self->set_input_parameters(sampleRate, channels);
}

EMSCRIPTEN_KEEPALIVE
int Encoder_input(audio::Encoder *self, const int16_t *data, int size)
{
    return self->input(data, size);
}

EMSCRIPTEN_KEEPALIVE
bool Encoder_output(audio::Encoder *self, String *out)
{
    return self->output(out);
}

// Decoder

EMSCRIPTEN_KEEPALIVE
audio::Decoder* Decoder_new(int codec, int sampleRate, int channels)
{
    LOGI("Decoder_new, "<<getCodecName(codec)<<","<<sampleRate<<"/"<<channels);
    return audio::CreateDecoder(codec, sampleRate, channels);
}

EMSCRIPTEN_KEEPALIVE
void Decoder_delete(audio::Decoder *self)
{
    delete self;
}

EMSCRIPTEN_KEEPALIVE
void Decoder_setOutputParameters(audio::Decoder *self, int sampleRate, int channels)
{
    self->set_output_parameters(sampleRate, channels);
}

EMSCRIPTEN_KEEPALIVE
int Decoder_input(audio::Decoder *self, const char* data, size_t size)
{
    return self->input(data,size);
}

EMSCRIPTEN_KEEPALIVE
bool Decoder_output(audio::Decoder *self, Int16Array *out)
{
    return self->output(out);
}

} // extern "C"
