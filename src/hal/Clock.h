// Wall clock. There is no NTP on a SoftAP with no uplink, so time arrives either from a
// DS3231 (not fitted in v1) or from the browser via POST /api/time.
#pragma once

#include <cstdint>

namespace dh {

class Clock {
  public:
    void begin();

    bool timeKnown() const { return known_; }
    void setEpoch(uint32_t epoch);

    // Seconds since the epoch, or 0 while the time is unknown.
    uint32_t epoch() const;
    uint32_t uptimeS() const;

  private:
    bool known_ = false;
    uint32_t epochAtSet_ = 0;
    uint32_t uptimeAtSet_ = 0;
};

}  // namespace dh
