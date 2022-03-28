#ifndef _SIGNAL_PROCESSING_H_
#define _SIGNAL_PROCESSING_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void MonoToStereo(const int16_t* src_audio, size_t samples_per_channel, int16_t* dst_audio);
void StereoToMono(const int16_t* src_audio, size_t samples_per_channel, int16_t* dst_audio);
int DoResample(const int16_t* src_audio, int src_len, int in_freq, int in_channels,
    int16_t* dst_audio, int* max_out_len, int out_freq, int out_channels);


namespace webrtc {
class Resampler;
}

class MyResampler {
    enum {
        kUnknown = 0,
        kNotTodo,
        kMonoToStereo,
        kStereoToMono,
        kConvert,
    };
public:
    MyResampler(int in_freq, int in_channels, int out_freq, int out_channels);
    ~MyResampler();

    void Reset(int in_freq, int in_channels, int out_freq, int out_channels);
    int ExpectMaxSize(size_t src_len);

    // thread-safe
    int Push(const int16_t* src_audio, size_t src_len, int16_t* dst_audio, size_t max_len);

    // not thread-safe
    int Push(const int16_t* src_audio, size_t src_len);
    const int16_t* Data() {return m_buffer;}

    int GetInFreq() {return m_in_freq;}
    int GetInChannels() {return m_in_channels;}

    int GetOutFreq() {return m_out_freq;}
    int GetOutChannels() {return m_out_channels;}

private:
    int16_t* AllocBuffer(int size);

private:
    int m_state;
    int m_in_freq;
    int m_in_channels;
    int m_out_freq;
    int m_out_channels;
    webrtc::Resampler* m_handle;

    int16_t* m_buffer;
    size_t m_buffer_size;
};


#endif // _SIGNAL_PROCESSING_H_
