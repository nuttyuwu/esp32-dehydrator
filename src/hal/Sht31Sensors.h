#pragma once

#include <Adafruit_SHT31.h>

#include "hal/ISensors.h"

namespace dh {

// Two SHT31s on one bus at 0x44 (bottom/inlet) and 0x45 (top/exhaust).
// Compiled into env:hw only. Unvalidated until the hardware exists.
class Sht31Sensors : public ISensors {
  public:
    bool begin() override;
    Reading readBottom() override;
    Reading readTop() override;

    bool bottomPresent() const { return botOk_; }
    bool topPresent() const { return topOk_; }

  private:
    Reading read(Adafruit_SHT31& dev, bool present);

    Adafruit_SHT31 bot_;
    Adafruit_SHT31 top_;
    bool botOk_ = false;
    bool topOk_ = false;
};

}  // namespace dh
