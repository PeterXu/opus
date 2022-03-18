#include "base/common.h"
#include <sys/time.h>

static String kCodecNames[] = {
    "unknown",
    "opus",
    "pcma",
    "pcmu",
};

bool isCodecValid(int codec) {
    return (codec > UnknownCodec && codec < AllCodec);
}

String getCodecName(int codec) {
    if (isCodecValid(codec)) {
        return kCodecNames[codec];
    } else {
        return "unknown";
    }
}

int64_t GetTimeMs(const struct timeval *tv) {
    return tv->tv_sec * 1000 + tv->tv_usec / 1000;
}

int64_t NowMs() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return GetTimeMs(&tv);
}

