// Filtering and validity tracking for the two T/RH sensors.
// Median-of-3 kills single-sample I2C glitches, the EMA takes the edge off the rest.
#pragma once

#include <cstdint>

#include "core/Types.h"

namespace dh {

class SensorHub {
  public:
    void begin(uint32_t nowMs);
    void update(const Reading& bot, const Reading& top, const Config& cfg, uint32_t nowMs);

    const Reading& bottom() const { return bot_; }
    const Reading& top() const { return top_; }
    bool anyValid() const { return bot_.valid || top_.valid; }

    // Unfiltered, offset-corrected readings. Safety's over-temp trip uses these: smoothing
    // a protective limit delays it by several seconds for no benefit. The SHT31 driver
    // already CRC-checks and range-checks, so a value that arrives at all is a real one.
    const Reading& rawBottom() const { return rawBot_; }
    const Reading& rawTop() const { return rawTop_; }

    // Control input: bottom (inlet) preferred, top as fallback. valid=false if neither.
    Reading control() const;

    uint32_t lastValidMs() const { return lastValidMs_; }
    uint32_t invalidForMs(uint32_t nowMs) const;

  private:
    class Channel {
      public:
        void reset() { n_ = 0; idx_ = 0; init_ = false; }
        float push(float v);

      private:
        float buf_[3] = {0, 0, 0};
        uint8_t n_ = 0;
        uint8_t idx_ = 0;
        bool init_ = false;
        float ema_ = 0.0f;
    };

    Channel fTBot_, fRhBot_, fTTop_, fRhTop_;
    Reading bot_, top_;
    Reading rawBot_, rawTop_;
    uint32_t lastValidMs_ = 0;
};

}  // namespace dh
