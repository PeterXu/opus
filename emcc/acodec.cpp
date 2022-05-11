#include "acodec.h"
#include <emscripten.h>

#include "opus.h"
#include "g711/g711_interface.h"
#include "base/signal_processing.h"
#include "base/buffer.h"

namespace audio {

///
/// audio encoder
///

class EmptyEncoder : public Encoder {
public:
    virtual ~EmptyEncoder() {}
    virtual void set_complexity(int complexity) {}
    virtual void set_bitrate(int bitrate) {}
    virtual int input(const int16_t* data, size_t size, int sampleRate, int channels) {return -1;}
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
        m_sampleRate(sampleRate), m_channels(channels), m_bitrate(bitrate), m_frameSize(frameSize)
    {}
    virtual ~BaseEncoder() {
        delete m_resampler;
        m_resampler = NULL;
        delete m_samples;
        m_samples = NULL;
        for (auto it=m_samples_list.begin(); it != m_samples_list.end(); it++) {
            MySamples *one = *it;
            delete one;
        }
        m_samples_list.clear();
    }

    // base interfaces
    virtual void set_complexity(int complexity) {}
    virtual void set_bitrate(int bitrate) {}

    bool init_resampler(int sampleRate, int channels) {
        if ((sampleRate == 0 && channels == 0) ||
            (sampleRate == m_sampleRate && channels == m_channels)) {
            if (m_resampler) {
                LOGI("[enc] resampler not needed, input="<<sampleRate<<"/"<<channels);
                delete m_resampler;
                m_resampler = NULL;
            }
            return true;
        }
        if (sampleRate < MIN_SAMPLE_RATE ||
            sampleRate > MAX_SAMPLE_RATE ||
            channels < 1 ||
            channels > 2) {
            return false;
        }
        if (m_resampler == NULL) {
            LOGI("[enc] resampler init, input="<<sampleRate<<"/"<<channels<<", output="<<m_sampleRate<<"/"<<m_channels);
            m_resampler = new MyResampler(sampleRate, channels, m_sampleRate, m_channels);
        } else if (m_resampler->GetInFreq() != sampleRate || m_resampler->GetInChannels() != channels) {
            LOGI("[enc] resampler reset, new input="<<sampleRate<<"/"<<channels);
            m_resampler->Reset(sampleRate, channels, m_sampleRate, m_channels);
        }
        return true;
    }
    int check_samples() {
        if (m_samples_list.size() >= MAX_SAMPLES_NUMBER) {
            int delta = std::max(int(m_frameSize-4), 5);
            //LOGI("now="<<NowMs()<<", last="<<m_last_output_time<<", delta="<<delta);
            return (NowMs() >= m_last_output_time + delta) ? 1 : 0;
        } else {
            return 0;
        }
    }
    void push_samples(MySamples *samples) {
        while (m_samples_list.size() > MAX_SAMPLES_NUMBER) {
            auto one = m_samples_list.front();
            delete one;
            m_samples_list.pop_front();
        }
        samples->compute_energy();
        m_samples_list.push_back(samples);
    }
    virtual int input(const int16_t* data, size_t size, int sampleRate, int channels) {
        if (!data || size <= 0) {
            return check_samples();
        }

        if (!init_resampler(sampleRate, channels)) {
            LOGE("[enc] input invalid parameters="<<sampleRate<<"/"<<channels);
            return -1;
        }

        uint32_t now = NowMs();
        if (!m_samples) {
            m_samples = new MySamples();
        }
        m_samples->append(data, size, now);

        size_t inputFrameSamplesSize = sampleRate / 1000.0 * m_frameSize * channels;
        if (!m_resampler) {
            inputFrameSamplesSize = getCodecFrameSamplesSize();
        }

        //LOGI("[enc] input samples size:"<<inputFrameSamplesSize","<<m_samples->size()<<","<<m_samples_list.size());
        if (m_samples->size() >= inputFrameSamplesSize) {
            auto samples = m_samples;

            // first save residual data as new one.
            m_samples = new MySamples();
            size_t residual = samples->size() - inputFrameSamplesSize;
            if (residual > 0) {
                m_samples->set(samples->data(inputFrameSamplesSize), residual, samples->last_time(), samples->last_time());
                samples->erase(inputFrameSamplesSize, -1);
            }

            // second resample if need
            if (m_resampler) {
                int iret = m_resampler->Push(samples->data(0), samples->size());
                if (iret > 0) {
                    const int16_t* newData = m_resampler->Data();
                    size_t codecFrameSamplesSize = getCodecFrameSamplesSize();

                    auto samples2 = new MySamples();
                    samples2->set(newData, codecFrameSamplesSize, samples->first_time(), samples->last_time());
                    push_samples(samples2);
                } else if (iret == 0) {
                    // nop and copy next
                    push_samples(samples);
                } else {
                    LOGE("[enc] input resampler failed, ret="<<iret<<", samples="<<samples->size());
                    delete samples;
                    return -1;
                }
            } else {
                // nop and copy next
                push_samples(samples);
            }
        }
        return check_samples();
    }
    virtual bool output(String *out) {
        if (!out || m_samples_list.empty()) {
            return false;
        }

        size_t iret = -1;
        size_t codecFrameSamplesSize = getCodecFrameSamplesSize();
        auto samples = m_samples_list.front();
        if (samples->size() >= codecFrameSamplesSize) {
            size_t bufferSize = getMaxEncodedSize();
            char *buffer = m_alloc.get(bufferSize); // should not be deleted
            iret = encodePacket(samples->data(), codecFrameSamplesSize, (uint8_t *)buffer, bufferSize);
            if (iret > 0) {
                out->assign(buffer, buffer + iret);
            } else {
                LOGE("[enc] encode error="<<iret);
            }
            delete samples;
            m_samples_list.pop_front();
            m_last_output_time = NowMs();
        }
        return (iret > 0);
    }

protected:
    virtual size_t getCodecFrameSamplesSize() {
        return static_cast<size_t>(m_sampleRate/1000.0*m_frameSize*m_channels);
    }
    virtual size_t getMaxEncodedSize() {
        return getCodecFrameSamplesSize() * 8;
    }
    virtual size_t encodePacket(Int16Array &samples, size_t sampleSize, uint8_t *buffer, size_t bufferSize) = 0;

protected:
    int m_codec;
    int m_sampleRate; // HZ
    int m_channels;
    int m_bitrate; // bps
    float m_frameSize; //ms

private:
    MyResampler *m_resampler = nullptr;
    MySamples *m_samples = nullptr;
    std::deque<MySamples *> m_samples_list;
    uint32_t m_last_output_time = 0;
    MyAlloc<char> m_alloc;
};

class CG711Encoder : public BaseEncoder {
public:
    CG711Encoder(int codec, float frameSize) : BaseEncoder(codec, 8000, 1, 0, frameSize)
    {}
    virtual ~CG711Encoder() {}
    virtual size_t encodePacket(Int16Array &samples, size_t sampleSize, uint8_t *buffer, size_t bufferSize) {
        size_t iret = 0;
        if (m_codec == PCMA) {
            iret = WebRtcG711_EncodeA(&(samples[0]), sampleSize, buffer);
        } else {
            iret = WebRtcG711_EncodeU(&(samples[0]), sampleSize, buffer);
        }
        if (iret <= 0) {
            LOGI("[enc] g711 encode error ret="<<iret<<", size="<<sampleSize<<"/"<<samples.size()<<"/"<<bufferSize);
        }
        return iret;
    }
};

class COpusEncoder : public BaseEncoder {
public:
    COpusEncoder(float frameSize, int sampleRate, int channels, int bitrate, bool isVoip)
        : BaseEncoder(OPUS, sampleRate, channels, bitrate, frameSize) {
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
            LOGE("[enc] opus create encoder error="<<err);
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
    virtual size_t encodePacket(Int16Array &samples, size_t sampleSize, uint8_t *buffer, size_t bufferSize) {
        int iret = 0;
        if (m_enc) {
            iret = opus_encode(m_enc, &samples[0], sampleSize/m_channels, buffer, bufferSize);
        }
        return size_t(iret);
    }

private:
    OpusEncoder *m_enc = nullptr;
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
    virtual int input(const char *data, size_t size, int seq) {return -1;}
    virtual bool output(Int16Array *out, int &sampleRate, int &channels, int &seq) {return false;}
};

class BaseDecoder : public Decoder {
public:
    BaseDecoder(int codec, int sampleRate, int channels)
        : m_codec(codec), m_sampleRate(sampleRate), m_channels(channels)
    {}
    virtual ~BaseDecoder() {
        delete m_resampler;
        m_resampler = NULL;
        for (auto it=m_packet_list.begin(); it != m_packet_list.end(); it++) {
            MyPacket *one = *it;
            delete one;
        }
        m_packet_list.clear();
    }
    bool init_resampler(int sampleRate, int channels) {
        if ((sampleRate == 0 && channels == 0) ||
            (sampleRate == m_sampleRate && channels == m_channels)) {
            if (m_resampler) {
                LOGI("[dec] resampler not needed, output="<<sampleRate<<"/"<<channels);
                delete m_resampler;
                m_resampler = NULL;
            }
            return true;
        }
        if (sampleRate < MIN_SAMPLE_RATE ||
            sampleRate > MAX_SAMPLE_RATE ||
            channels < 1 ||
            channels > 2) {
            return false;
        }
        if (m_resampler == NULL) {
            LOGI("[dec] resampler init, input="<<m_sampleRate<<"/"<<m_channels<<", output="<<sampleRate<<"/"<<channels);
            m_resampler = new MyResampler(m_sampleRate, m_channels, sampleRate, channels);
        } else {
            LOGI("[dec] resampler reset, new output="<<sampleRate<<"/"<<channels);
            m_resampler->Reset(m_sampleRate, m_channels, sampleRate, channels);
        }
        return true;
    }
    int check_packet() {
        uint32_t now = NowMs();
        // support min frame-size 10ms.
        int frameSize = std::max(int(m_frameSize+0.5f), 10);

        int minTimeout = 100;
        int minNumber = minTimeout/frameSize;
        int maxTimeout = 3000;
        int maxNumber = maxTimeout/frameSize;
        int midTimeout = minTimeout + frameSize*2;
        int midNumber = midTimeout/frameSize;

        int iret = 0;
        while(!m_packet_list.empty()) {
            auto packet = m_packet_list.front();
            if (now >= packet->time() + maxTimeout || m_packet_list.size() >= maxNumber) {
                delete packet;
                m_packet_list.pop_front();
            } else {
                if (now >= packet->time() + midTimeout || m_packet_list.size() >= midNumber) {
                    int delta = frameSize * 0.4f;
                    iret = (now >= m_last_output_time + delta) ? 2 : 0;
                } else if (now >= packet->time() + minTimeout || m_packet_list.size() >= minNumber) {
                    int delta = frameSize * 0.8f;
                    iret = (now >= m_last_output_time + delta) ? 1 : 0;
                }
                break;
            }
        }

        return iret;
    }
    virtual int input(const char *data, size_t size, int seq) {
        uint32_t now = NowMs();
        if (data != NULL && size > 0) {
            auto packet = new MyPacket(seq);
            packet->set(data, size, now);
            m_packet_list.push_back(packet);
        }
        return check_packet();
    }
    virtual bool output(Int16Array *out, int &sampleRate, int &channels, int &seq) {
        if (!out || m_packet_list.empty()) {
            return false;
        }

        if (!init_resampler(sampleRate, channels)) {
            LOGE("[dec] output invalid parameters="<<sampleRate<<"/"<<channels);
            return false;
        }

        size_t bufferSize = getMaxDecodedSize();
        int16_t *buffer = m_alloc.get(bufferSize); // should not be deleted
        auto packet = m_packet_list.front();
        seq = packet->seq();

        size_t iret = decodePacket(packet->data(), buffer, bufferSize);
        if (iret > 0) {
            iret = iret * m_channels; // total output samples
            //LOGI("[dec] output samples size="<<iret<<", channels="<<channels<<", "<<m_channels<<","<<bufferSize);
            if (m_resampler) {
                int iret2 = m_resampler->Push(buffer, iret);
                if (iret2 > 0) {
                    const int16_t* newData = m_resampler->Data();
                    out->assign(newData, newData + iret2);
                    iret = iret2;
                } else if (iret2 == 0) {
                    out->assign(buffer, buffer + iret);
                } else {
                    LOGE("[dec] input resampler failed");
                    iret = -1;
                }
            } else {
                out->assign(buffer, buffer + iret);
            }
        } else {
            LOGE("[dec] decode error="<<iret);
        }

        if (iret > 0) {
            //uint32_t energy = ComputeAudioLevel(&((*out)[0]), out->size());
            if (m_resampler) {
                sampleRate = m_resampler->GetOutFreq();
                channels = m_resampler->GetOutChannels();
            } else {
                sampleRate = m_sampleRate;
                channels = m_channels;
            }
            m_frameSize = iret/(sampleRate/1000.0*channels);
        }

        delete packet;
        m_packet_list.pop_front();
        m_last_output_time = NowMs();
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
    MyResampler *m_resampler = nullptr;
    std::deque<MyPacket *> m_packet_list;
    float m_frameSize = 0.0f;
    uint32_t m_last_output_time = 0;
    MyAlloc<int16_t> m_alloc;
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
    COpusDecoder(int sampleRate, int channels) : BaseDecoder(OPUS, sampleRate, channels) {
        int err = 0;
        m_dec = opus_decoder_create(sampleRate, channels, &err);
        if (m_dec == NULL) {
            LOGE("[dec] create opus decode error="<<err);
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
    OpusDecoder *m_dec = nullptr;
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
int Encoder_input(audio::Encoder *self, const int16_t *data, int size, int sampleRate, int channels)
{
    return self->input(data, size, sampleRate, channels);
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
int Decoder_input(audio::Decoder *self, const char* data, int size)
{
    return self->input(data,size,0);
}

EMSCRIPTEN_KEEPALIVE
bool Decoder_output(audio::Decoder *self, Int16Array *out, int sampleRate, int channels)
{
    int seq = 0;
    return self->output(out, sampleRate, channels, seq);
}

} // extern "C"
