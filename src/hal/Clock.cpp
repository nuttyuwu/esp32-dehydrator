#include "hal/Clock.h"

#include <Arduino.h>
#include <sys/time.h>

namespace dh {

void Clock::begin() {
    known_ = false;
    epochAtSet_ = 0;
    uptimeAtSet_ = 0;
}

uint32_t Clock::uptimeS() const { return static_cast<uint32_t>(millis() / 1000ULL); }

void Clock::setEpoch(uint32_t epoch) {
    if (epoch < 1700000000UL) return;   // before 2023 — obviously junk, ignore it
    epochAtSet_ = epoch;
    uptimeAtSet_ = uptimeS();
    known_ = true;

    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(epoch);
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
}

uint32_t Clock::epoch() const {
    if (!known_) return 0;
    return epochAtSet_ + (uptimeS() - uptimeAtSet_);
}

}  // namespace dh
