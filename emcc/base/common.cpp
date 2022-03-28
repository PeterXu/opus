#include "base/common.h"
#include <sys/time.h>
#include <time.h>
#include <cmath>

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


namespace {
static const int kMinLevelDb = 127;
static const float kMaxSquaredLevel = 32768 * 32768;
static const float kMinLevel = 1.995262314968883e-13f;

// mean-square => audio-level
static int ComputeRms(float mean_square) {
    if (mean_square <= kMinLevel * kMaxSquaredLevel) {
        // Very faint; simply return the minimum value.
        return kMinLevelDb;
    }
    // Normalize by the max level.
    const float mean_square_norm = mean_square / kMaxSquaredLevel;
    // 20log_10(x^0.5) = 10log_10(x), where x is mean_square_norm.
    float rms = 10.f * std::log10(mean_square_norm);
    // rms range: [-127f, 0.f]
    if (rms > 0.f)
        rms = 0.f;
    else if (rms < -kMinLevelDb)
        rms = -kMinLevelDb;
    // Return the negated value.
    return static_cast<int>(-rms + 0.5f);
}

// audio-level => mean-square
static float ComputeMs(int level) {
    // 10^(-y / 10.f), where y is rms.
    const float rms = -level;
    float mean_square_norm = std::powf(10, (rms/10.f));
    float mean_square = mean_square_norm * kMaxSquaredLevel;
    return mean_square;
}

float ComputeMeanSquare(int16_t* data, size_t size) {
    uint32_t sum = 0;
    for (size_t i=0; i<size; i++) {
        sum += data[i] * data[i];
    }
    return float(sum * 1.0f / size);
}

} // namespace

int ComputeAudioLevel(int16_t* data, size_t size) {
    float mean_square = ComputeMeanSquare(data, size);
    int rms = ComputeRms(mean_square);
    return rms;
}
