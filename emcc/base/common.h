#ifndef _BASE_COMMON_H_
#define _BASE_COMMON_H_

#include <string>
#include <vector>
#include <deque>
#include <iostream>
#include <algorithm>

#include <stdint.h>

typedef std::string String;
typedef std::vector<uint8_t> Uint8Array;
typedef std::vector<int16_t> Int16Array;
typedef std::vector<float> Float32Array;

#define LOGI(...) std::cout<<__VA_ARGS__<<std::endl
#define LOGE(...) std::cerr<<__VA_ARGS__<<std::endl

#define MAX_SAMPLES_NUMBER (5)
#define MAX_PACKET_NUMBER  (5)

#define MIN_SAMPLE_RATE (8000)
#define MAX_SAMPLE_RATE (96000)

enum AVCodecs {
    UnknownCodec = 0,
    OPUS,
    PCMA,
    PCMU,
    AllCodec,
};
bool isCodecValid(int codec);
String getCodecName(int codec);

uint32_t NowMs();
int RandNumber();
uint32_t RandTimestamp();
int ComputeAudioLevel(int16_t* data, size_t size);

#endif // _BASE_COMMON_H_
