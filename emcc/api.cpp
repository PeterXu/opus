#include <iostream>
#include <vector>
#include <deque>
#include <stdint.h>

#include <emscripten.h>
#include <opus.h>
#include <g711/g711_interface.h>

typedef std::string String;
typedef std::vector<int16_t> Int16Array;
typedef std::vector<uint8_t> Uint8Array;

#define LOGI(...) std::cout<<__VA_ARGS__<<std::endl
#define LOGE(...) std::cerr<<__VA_ARGS__<<std::endl

enum Codecs {
    OPUS = 1,
    PCMA = 2,
    PCMU = 3,
};
static String kCodecs[] = {
    "opus",
    "pcma",
    "pcmu",
};
static String codecName(int idx) {
    if (idx > 0 && idx <= 3) {
        return kCodecs[idx-1];
    } else {
        return "unknown";
    }
}


/// audio encoder

class Encoder {
public:
    Encoder(int codec, int sampleRate, int channels, int bitrate, float frameSize)
        : m_codec(codec),
        m_sampleRate(sampleRate), m_channels(channels), m_bitrate(bitrate), m_frameSize(frameSize)
    {}
    virtual ~Encoder() {
        m_samples.clear();
    }
    virtual void set_complexity(int complexity) {}
    virtual void set_bitrate(int bitrate) {}
    virtual void input(const int16_t *data, int size) {
        Int16Array samples(data, data+size);
        m_samples.push_back(samples);
    }
    virtual bool output(String *out) {
        if (m_samples.empty()) {
            return false;
        }

        size_t sampleSize = getRawSampleSize();
        std::deque<Int16Array>::reference sample = m_samples.front();
        if (sampleSize > sample.size()) {
            m_samples.pop_front();
            return false;
        }

        size_t bufferSize = getMaxEncodedSize();
        uint8_t *buffer = new uint8_t[bufferSize];
        size_t iret = encodePacket(sample, sampleSize, buffer, bufferSize);
        if (iret > 0) {
            out->assign((char *)buffer, iret);
        }

        delete [] buffer;
        m_samples.pop_front();
        return (iret > 0);
    }

protected:
    virtual size_t getRawSampleSize() {
        return static_cast<size_t>(m_sampleRate/1000.0*m_frameSize*m_channels);
    }
    virtual size_t getMaxEncodedSize() {
        return getRawSampleSize() * 2;
    }
    virtual size_t encodePacket(Int16Array &sample, int sampleSize, uint8_t *buffer, size_t bufferSize) = 0;

protected:
    int m_codec;
    int m_sampleRate; // HZ
    int m_channels;
    int m_bitrate; // bps
    float m_frameSize; //ms

private:
    std::deque<Int16Array> m_samples;
};

class CG711Encoder : public Encoder {
public:
    CG711Encoder(int codec, float frameSize) : Encoder(codec, 8000, 1, 0, frameSize)
    {}
    virtual ~CG711Encoder() {}
    virtual size_t encodePacket(Int16Array &sample, int sampleSize, uint8_t *buffer, size_t bufferSize)
    {
        size_t iret = 0;
        if (m_codec == PCMA) {
            iret = WebRtcG711_EncodeA(&(sample[0]), sample.size(), buffer);
        } else {
            iret = WebRtcG711_EncodeU(&(sample[0]), sample.size(), buffer);
        }
        return iret;
    }
};

class COpusEncoder : public Encoder {
public:
    COpusEncoder(float frameSize, int sampleRate, int channels, int bitrate, bool isVoip)
        : Encoder(OPUS, sampleRate, channels, bitrate, frameSize), m_enc(NULL)
    {
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
    virtual ~COpusEncoder()
    {
        if (m_enc) {
            opus_encoder_destroy(m_enc);
            m_enc = NULL;
        }
    }
    virtual void set_complexity(int complexity)
    {
        if (m_enc != NULL) {
            opus_encoder_ctl(m_enc, OPUS_SET_COMPLEXITY(complexity));
        }
    }
    virtual void set_bitrate(int bitrate)
    {
        if (m_enc != NULL){
            if (bitrate <= 0) {
                opus_encoder_ctl(m_enc, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));
            } else {
                opus_encoder_ctl(m_enc, OPUS_SET_BITRATE(bitrate));
            }
        }
    }
    virtual size_t getMaxEncodedSize()
    {
        // Calculate the number of bytes we expect the encoder to produce,
        // then multiply by two to give a wide margin for error.
        const size_t bytes_per_ms = static_cast<size_t>(m_bitrate/(1000*8) + 1);
        const size_t num_10ms_frames_per_packet = static_cast<size_t>(m_frameSize / 10);
        const size_t approx_encoded_bytes = num_10ms_frames_per_packet * 10 * bytes_per_ms;
        return 2 * approx_encoded_bytes;
    }
    virtual size_t encodePacket(Int16Array &sample, int sampleSize, uint8_t *buffer, size_t bufferSize)
    {
        size_t iret = 0;
        if (m_enc) {
            iret = opus_encode(m_enc, &sample[0], sampleSize/m_channels, buffer, bufferSize);
        }
        return iret;
    }

private:
    OpusEncoder *m_enc;
};

/// audio decoder

class Decoder {
public:
    Decoder(int codec, int sampleRate, int channels)
        : m_codec(codec), m_sampleRate(sampleRate), m_channels(channels)
    {}
    virtual ~Decoder() {}
    virtual void input(const char *data, size_t size) {
        Uint8Array packet(data, data+size);
        m_packets.push_back(packet);
    }
    virtual bool output(Int16Array *out)
    {
        if (m_packets.empty()) {
            return false;
        }

        size_t bufferSize = getMaxDecodedSize();
        int16_t *buffer = new int16_t[bufferSize];
        std::deque<Uint8Array>::reference packet = m_packets.front();

        size_t iret = decodePacket(packet, buffer, bufferSize);
        if (iret > 0){
            *out = Int16Array(buffer, buffer+iret);
        }

        m_packets.pop_front();
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
    std::deque<Uint8Array> m_packets;
};

class CG711Decoder : public Decoder {
public:
    CG711Decoder(int codec) : Decoder(codec, 8000, 1) {}
    virtual ~CG711Decoder() {}
    virtual size_t decodePacket(Uint8Array &packet, int16_t *buffer, size_t bufferSize)
    {
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

class COpusDecoder : public Decoder {
public:
    COpusDecoder(int sampleRate, int channels)
        : Decoder(OPUS, sampleRate, channels), m_dec(NULL)
    {
        int err = 0;
        m_dec = opus_decoder_create(sampleRate, channels, &err);
        if (m_dec == NULL) {
            LOGE("[opus-dec] error while creating opus decoder: "<<err);
        }
    }
    virtual ~COpusDecoder()
    {
        if (m_dec) {
            opus_decoder_destroy(m_dec);
            m_dec = NULL;
        }
    }
    virtual size_t decodePacket(Uint8Array &packet, int16_t *buffer, size_t bufferSize)
    {
        int iret = 0;
        if (m_dec) {
            iret = opus_decode(m_dec, &packet[0], packet.size(), buffer, bufferSize/m_channels, 0);
        }
        return size_t(iret);
    }

private:
    OpusDecoder *m_dec;
};


extern "C"{

// Encoder

EMSCRIPTEN_KEEPALIVE
Encoder* Encoder_new(int codec, float frameSize, int sampleRate, int channels, int bitrate, bool isVoip)
{
    LOGI("Encoder_new, "<<codecName(codec)<<","<<frameSize<<"ms,"<<sampleRate<<"/"<<channels<<","<<bitrate<<"bps,"<<isVoip);
    Encoder *self = NULL;
    if (codec == OPUS) {
        self = new COpusEncoder(frameSize, sampleRate, channels, bitrate, isVoip);
    } else if (codec == PCMU || codec == PCMA) {
        self = new CG711Encoder(codec, frameSize);
    }
    return self;
}

EMSCRIPTEN_KEEPALIVE
void Encoder_delete(Encoder *self)
{
    delete self;
}

EMSCRIPTEN_KEEPALIVE
void Encoder_setComplexity(Encoder *self, int complexity)
{
    if (self) self->set_complexity(complexity);
}

EMSCRIPTEN_KEEPALIVE
void Encoder_setBitrate(Encoder *self, int bitrate)
{
    if (self) self->set_bitrate(bitrate);
}

EMSCRIPTEN_KEEPALIVE
void Encoder_input(Encoder *self, const int16_t *data, int size)
{
    if (self) self->input(data, size);
}

EMSCRIPTEN_KEEPALIVE
bool Encoder_output(Encoder *self, String *out)
{
    if (self) return self->output(out);
    else return false;
}

// Decoder

EMSCRIPTEN_KEEPALIVE
Decoder* Decoder_new(int codec, int sampleRate, int channels)
{
    LOGI("Decoder_new, "<<codecName(codec)<<","<<sampleRate<<"/"<<channels);
    Decoder *self = NULL;
    if (codec == OPUS) {
        self = new COpusDecoder(sampleRate, channels);
    } else if (codec == PCMU || codec == PCMA) {
        self = new CG711Decoder(codec);
    } 
    return self;
}

EMSCRIPTEN_KEEPALIVE
void Decoder_delete(Decoder *self)
{
    delete self;
}

EMSCRIPTEN_KEEPALIVE
void Decoder_input(Decoder *self, const char* data, size_t size)
{
    self->input(data,size);
}

EMSCRIPTEN_KEEPALIVE
bool Decoder_output(Decoder *self, Int16Array *out)
{
    return self->output(out);
}

// String

EMSCRIPTEN_KEEPALIVE
size_t String_size(String *self)
{
    return self->size();
}

EMSCRIPTEN_KEEPALIVE
String* String_new()
{
    return new std::string();
}

EMSCRIPTEN_KEEPALIVE
const char* String_data(String *self)
{
    return self->c_str();
}

EMSCRIPTEN_KEEPALIVE
void String_delete(String *self)
{
    delete self;
}

// Int16Array

EMSCRIPTEN_KEEPALIVE
size_t Int16Array_size(Int16Array *self)
{
    return self->size();
}

EMSCRIPTEN_KEEPALIVE
Int16Array* Int16Array_new()
{
    return new std::vector<int16_t>();
}

EMSCRIPTEN_KEEPALIVE
const int16_t* Int16Array_data(Int16Array *self)
{
    return &(*self)[0];
}

EMSCRIPTEN_KEEPALIVE
void Int16Array_delete(Int16Array *self)
{
    delete self;
}

}

