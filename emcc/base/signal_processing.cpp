#include "base/signal_processing.h"
#include "spl/resampler.h"

void MonoToStereo(const int16_t* src_audio, size_t samples_per_channel, int16_t* dst_audio) {
  for (size_t i = 0; i < samples_per_channel; i++) {
    dst_audio[2 * i] = src_audio[i];
    dst_audio[2 * i + 1] = src_audio[i];
  }
}

void StereoToMono(const int16_t* src_audio, size_t samples_per_channel, int16_t* dst_audio) {
    for (size_t i = 0; i < samples_per_channel; i++) {
        dst_audio[i] = (src_audio[2 * i] + src_audio[2 * i + 1]) >> 1;
    }
}

int ComputeGcd(int a, int b) {
    int c = a % b;
    while (c != 0) {
        a = b;
        b = c;
        c = a % b;
    }
    return b;
}

// return -1, failed
// return  0, nop,
// return >0, the number of output samples.
int DoResample(const int16_t* src_audio, int src_len, int in_freq, int in_channels,
    int16_t *dst_audio, int *max_out_len, int out_freq, int out_channels) {
    int max_len = *max_out_len;
    MyResampler *sampler = new MyResampler(in_freq, in_channels, out_freq, out_channels);
    int iret = sampler->Push(src_audio, src_len, dst_audio, max_len);
    delete sampler;
    return iret;
}


/// MyResampler

MyResampler::MyResampler(int in_freq, int in_channels, int out_freq, int out_channels) {
    m_state = kUnknown;
    m_handle = NULL;
    m_buffer = NULL;
    m_buffer_size = 0;
    Reset(in_freq, in_channels, out_freq, out_channels);
}

MyResampler::~MyResampler() {
    delete m_handle;
    m_handle = NULL;

    delete m_buffer;
    m_buffer = NULL;
}

void MyResampler::Reset(int in_freq, int in_channels, int out_freq, int out_channels) {
    m_state = kUnknown;
    m_in_freq = in_freq;
    m_in_channels = in_channels;
    m_out_freq = out_freq;
    m_out_channels = out_channels;

    // check channels
    if ((in_channels == 1 || in_channels == 2) &&
        (out_channels == 1 || out_channels == 2)) {
        if (in_freq == out_freq) {
            if (in_channels == out_channels) {
                m_state = kNotTodo;
            } else if (in_channels == 1) {
                m_state = kMonoToStereo;
            } else {
                m_state = kStereoToMono;
            }
        } else {
            m_state = kConvert;
            webrtc::ResamplerType type = (out_channels == 1) ? 
                webrtc::kResamplerSynchronous :
                webrtc::kResamplerSynchronousStereo;
            if (m_handle == NULL) {
                m_handle = new webrtc::Resampler(in_freq, out_freq, type);
            } else {
                m_handle->Reset(in_freq, out_freq, type);
            }
        }
    }
}

int MyResampler::ExpectMaxSize(size_t src_len) {
    const int samples_mono = (m_in_channels == 1) ? src_len : (src_len / 2);
    const int samples_stereo = samples_mono * 2;

    switch (m_state) {
    case kUnknown:
        break;
    case kNotTodo:
        return 0;
    case kMonoToStereo:
        return samples_stereo;
    case kStereoToMono:
        return samples_mono;
    case kConvert:
        return (src_len * (m_out_channels * m_out_freq) / (m_in_channels * m_in_freq)) * 2;
    default:
        break;
    }
    return -1;
}

int MyResampler::Push(const int16_t* src_audio, size_t src_len, int16_t* dst_audio, size_t max_len) {
    const int samples_mono = (m_in_channels == 1) ? src_len : (src_len / 2);
    const int samples_stereo = samples_mono * 2;

    switch (m_state) {
    case kUnknown:
        break;
    case kNotTodo:
        return 0;
    case kMonoToStereo:
        if (max_len >= samples_stereo) {
            MonoToStereo(src_audio, samples_mono, dst_audio);
            return samples_stereo;
        }
        break;
    case kStereoToMono:
        if (max_len < samples_mono) {
            StereoToMono(src_audio, samples_mono, dst_audio);
            return samples_mono;
        }
        break;
    case kConvert:
        {
            int16_t *samplesIn = (int16_t *)src_audio;
            int lengthIn = src_len;
            if (m_in_channels != m_out_channels) {
                // first convert to the same channels
                lengthIn = (m_in_channels == 1) ? samples_stereo : samples_mono;
                samplesIn = new int16_t[lengthIn];
                if (m_in_channels == 1) {
                    MonoToStereo(src_audio, samples_mono, samplesIn);
                } else {
                    StereoToMono(src_audio, samples_mono, samplesIn);
                }
            }

            int out_len = 0;
            int iret = m_handle->Push(samplesIn, lengthIn, dst_audio, max_len, out_len);

            if (m_in_channels != m_out_channels) {
                delete samplesIn;
            }
            if (iret == 0) {
                return out_len;
            }
        }
        break;
    default:
        break;
    }
    return -1;
}

int MyResampler::Push(const int16_t* src_audio, size_t src_len) {
    int max_len = ExpectMaxSize(src_len);
    if (max_len > 0) {
        int16_t* dst_audio = AllocBuffer(max_len);
        return Push(src_audio, src_len, dst_audio, max_len);
    } else {
        return max_len;
    }
}

int16_t* MyResampler::AllocBuffer(int size) {
    if (size > m_buffer_size) {
        delete m_buffer;
        m_buffer = new int16_t[size];
        m_buffer_size = size;
    }
    return m_buffer;
}

