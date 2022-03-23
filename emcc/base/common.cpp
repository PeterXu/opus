#include "base/common.h"
#include <sys/time.h>
#include <time.h>

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

uint32_t NowMs() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    uint32_t time = uint32_t(tv.tv_sec * 1000) + uint32_t(tv.tv_usec / 1000);
    return time;
}

int RandNumber() {
    srand (time(NULL));
    return rand();
}

uint32_t RandTimestamp() {
    srand (time(NULL));
    uint32_t ts = 0;
    ts = rand();
    ts = ts << 16;
    ts += rand();
    ts &= 0x3fffffff;
    return ts;
}
