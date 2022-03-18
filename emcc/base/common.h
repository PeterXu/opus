#ifndef _BASE_COMMON_H_
#define _BASE_COMMON_H_

#include <string>
#include <vector>
#include <deque>
#include <iostream>

#include <stdint.h>

typedef std::string String;
typedef std::vector<uint8_t> Uint8Array;
typedef std::vector<int16_t> Int16Array;

#define LOGI(...) std::cout<<__VA_ARGS__<<std::endl
#define LOGE(...) std::cerr<<__VA_ARGS__<<std::endl

#define MAX_SAMPLES_NUMBER (50*5)
#define MAX_PACKET_NUMBER  (50*5)

enum AVCodecs {
    UnknownCodec = 0,
    OPUS,
    PCMA,
    PCMU,
    AllCodec,
};
bool isCodecValid(int codec);
String getCodecName(int codec);

int64_t NowMs();


#endif // _BASE_COMMON_H_
