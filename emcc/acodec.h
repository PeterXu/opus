#ifndef _ACODEC_H_
#define _ACODEC_H_

#include "base/common.h"

namespace audio {

class Encoder {
public:
    virtual ~Encoder() {}
    virtual void set_complexity(int complexity) = 0;
    virtual void set_bitrate(int bitrate) = 0;
    virtual void set_input_parameters(int sampleRate, int channels) = 0;
    virtual void input(const int16_t* data, int size) = 0;
    virtual bool output(String *out) = 0;
};
Encoder* CreateEncoder(int codec, float frameSize, int sampleRate, int channels, int bitrate, bool isVoip);

class Decoder {
public:
    virtual ~Decoder() {}
    virtual void set_output_parameters(int sampleRate, int channels) = 0;
    virtual void input(const char *data, size_t size) = 0;
    virtual bool output(Int16Array *out) = 0;
};
Decoder* CreateDecoder(int codec, int sampleRate, int channels);

} // namespace audio

#endif // _ACODEC_H_
