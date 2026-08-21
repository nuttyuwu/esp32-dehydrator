// Heater hysteresis with minimum on/off dwell. ARCHITECTURE.md section 6.
//
// ON  at <= setpoint - hysteresis/2   (43.5 C with the defaults)
// OFF at >= setpoint + hysteresis/2   (44.5 C)
//
// A bare "T < 44" comparison chatters the relay several times a minute; the band plus
// the dwell timers is what keeps the SSR and the element alive.
#pragma once

#include <cstdint>

#include "core/Types.h"

namespace dh {

class Controller {
  public:
    void begin(uint32_t nowMs);

    // allowHeat=false forces the heater off immediately, ignoring minOn — safety wins.
    // Returns the heater request (before the actuator interlock has its say).
    bool update(const Config& cfg, float tCtrl, bool tValid, bool allowHeat, uint32_t nowMs);

    bool heater() const { return heater_; }

    // True while the 60 s stack-fan post-purge after a heater-off is still running.
    bool inPurge(uint32_t nowMs) const;

    void forceOff(uint32_t nowMs);

    float onThreshold(const Config& cfg) const { return cfg.setpointC - cfg.hysteresisC / 2.0f; }
    float offThreshold(const Config& cfg) const { return cfg.setpointC + cfg.hysteresisC / 2.0f; }

  private:
    bool heater_ = false;
    uint32_t lastOnMs_ = 0;    // when the heater last switched ON
    uint32_t lastOffMs_ = 0;   // when it last switched OFF
    bool everOff_ = false;
};

}  // namespace dh
