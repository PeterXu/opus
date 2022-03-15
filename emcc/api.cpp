#include <emscripten.h>
#include <opus.h>
#include <stdint.h>
#include <iostream>
#include <vector>

typedef std::string String;
typedef std::vector<int16_t> Int16Array;

class Encoder {
public:
    Encoder(int channels, long int samplerate, long int bitrate, float frame_size, bool is_voip)
        : m_enc(NULL), m_samplerate(samplerate), m_channels(channels), m_bitrate(bitrate), m_frame_size(frame_size)
    {
        int err;
        int application = (is_voip ? OPUS_APPLICATION_VOIP : OPUS_APPLICATION_AUDIO);
        m_enc = opus_encoder_create(samplerate, channels, application, &err);
        if (m_enc != NULL){
            if (bitrate <= 0) {
                opus_encoder_ctl(m_enc, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));
            } else {
                opus_encoder_ctl(m_enc, OPUS_SET_BITRATE(bitrate));
            }
        } else {
            std::cerr << "[libopusjs] error while creating opus encoder: " << err << std::endl;
        }
    }

    void set_complexity(long int complexity)
    {
        if (m_enc != NULL) {
            opus_encoder_ctl(m_enc, OPUS_SET_COMPLEXITY(complexity));
        }
    }

    void set_bitrate(long int bitrate)
    {
        if (m_enc != NULL){
            if (bitrate <= 0) {
                opus_encoder_ctl(m_enc, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));
            } else {
                opus_encoder_ctl(m_enc, OPUS_SET_BITRATE(bitrate));
            }
        }
    }

    void input(const int16_t *data, int size)
    {
        std::vector<int16_t> samples(data, data+size);
        m_samples.insert(m_samples.end(), samples.begin(), samples.end());
    }

    size_t SufficientOutputBufferSize()
    {
        // Calculate the number of bytes we expect the encoder to produce,
        // then multiply by two to give a wide margin for error.
        const size_t bytes_per_ms = static_cast<size_t>(m_bitrate/(1000*8) + 1);
        const size_t num_10ms_frames_per_packet = static_cast<size_t>(m_frame_size / 10);
        const size_t approx_encoded_bytes = num_10ms_frames_per_packet * 10 * bytes_per_ms;
        return 2 * approx_encoded_bytes;
    }

    size_t GetRawSampleSize()
    {
        return static_cast<size_t>(m_samplerate*m_frame_size*m_channels/1000.0);
    }

    bool output(String *out)
    {
        if (!m_enc || !out) {
            return false;
        }

        long int sample_size = GetRawSampleSize();
        if (sample_size <= 0 || sample_size > m_samples.size()) {
            return false;
        }

        size_t max_size = SufficientOutputBufferSize();
        uint8_t *buffer = new uint8_t[max_size];

        long int iret = opus_encode(m_enc, &m_samples[0], sample_size/m_channels, buffer, max_size);
        if (iret > 0) {
            out->assign((char *)buffer, iret);
        }

        delete [] buffer;
        m_samples.erase(m_samples.begin(), m_samples.begin() + sample_size);

        return (iret > 0);
    }

    ~Encoder()
    {
        if (m_enc) {
            opus_encoder_destroy(m_enc);
            m_enc = NULL;
        }
    }

private:
    float m_frame_size; //ms
    long int m_bitrate; //bps
    long int m_samplerate;
    int m_channels;
    OpusEncoder *m_enc;
    std::vector<int16_t> m_samples;
};


class Decoder {
public:
    Decoder(int channels, long int samplerate) : m_dec(NULL), m_samplerate(samplerate), m_channels(channels)
    {
        int err;
        m_dec = opus_decoder_create(samplerate, channels, &err);
        if (m_dec == NULL) {
            std::cerr << "[libopusjs] error while creating opus decoder: " << err << std::endl;
        }
    }

    void input(const char *data, size_t size)
    {
        m_packets.push_back(std::string(data,size));
    }

    size_t MaxDecodedFrameSizeMs() {
        return m_samplerate/1000.0*120*m_channels; // 120ms max
    }

    bool output(Int16Array *out)
    {
        if (!m_dec || !out || m_packets.empty()) {
            return false;
        }

        long int buffer_size = MaxDecodedFrameSizeMs();
        int16_t *buffer = new int16_t[buffer_size];

        std::string &packet = m_packets[0];
        int iret = opus_decode(m_dec, (const uint8_t*)packet.c_str(), packet.size(), buffer, buffer_size/m_channels, 0);
        if (iret > 0){
            *out = std::vector<int16_t>(buffer, buffer+iret*m_channels);
        }

        m_packets.erase(m_packets.begin());
        delete [] buffer;

        return (iret > 0);
    }

    ~Decoder()
    {
        if (m_dec) {
            opus_decoder_destroy(m_dec);
            m_dec = NULL;
        }
    }

private:
    long int m_samplerate;
    int m_channels;
    OpusDecoder *m_dec;
    std::vector<std::string> m_packets;
};


extern "C"{

// Encoder

EMSCRIPTEN_KEEPALIVE
Encoder* Encoder_new(int channels, long int samplerate, long int bitrate, float frame_size, bool is_voip)
{
    return new Encoder(channels, samplerate, bitrate, frame_size, is_voip);
}

EMSCRIPTEN_KEEPALIVE
void Encoder_delete(Encoder *self)
{
    delete self;
}

EMSCRIPTEN_KEEPALIVE
void Encoder_setComplexity(Encoder *self, int complexity)
{
    self->set_complexity(complexity);
}

EMSCRIPTEN_KEEPALIVE
void Encoder_setBitrate(Encoder *self, int bitrate)
{
    self->set_bitrate(bitrate);
}

EMSCRIPTEN_KEEPALIVE
void Encoder_input(Encoder *self, const int16_t *data, int size)
{
    self->input(data, size);
}

EMSCRIPTEN_KEEPALIVE
bool Encoder_output(Encoder *self, String *out)
{
    return self->output(out);
}

// Decoder

EMSCRIPTEN_KEEPALIVE
Decoder* Decoder_new(int channels, long int samplerate)
{
    return new Decoder(channels, samplerate);
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

