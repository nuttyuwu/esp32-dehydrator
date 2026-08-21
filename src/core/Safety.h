// Latched fault detection. ARCHITECTURE.md section 6.
//
// Every trip here drives the heater off and the fans on, then latches until an explicit
// reset. Nothing in the UI or the TUI can bypass it — MANUAL mode passes through Safety
// exactly like AUTO does.
//
// This is the *software* layer. The mechanical thermal fuse in series with the element is
// what actually protects the cabinet; see ARCHITECTURE.md section 0.
#pragma once

#include <cstdint>

#include "core/SensorHub.h"
#include "core/Types.h"

namespace dh {

class Safety {
  public:
    void begin(uint32_t nowMs, bool watchdogReset);

    // Call once per control tick, before the state machine decides anything.
    // heaterCommanded is what the controller asked for on the *previous* tick.
    Fault evaluate(const SensorHub& hub, bool klixonOpen, bool heaterCommanded, uint32_t nowMs);

    void reset(uint32_t nowMs);

    bool latched() const { return latched_; }
    Fault fault() const { return fault_; }

  private:
    void trip(Fault f);

    bool latched_ = false;
    Fault fault_ = Fault::None;

    // Heater-ineffective detector: element open, SSR dead, or the fan is blowing the heat
    // straight out. Armed while the heater is continuously commanded on.
    bool heaterArmed_ = false;
    uint32_t heaterOnSinceMs_ = 0;
    float tAtHeaterOn_ = 0.0f;
    bool tAtHeaterOnValid_ = false;
};

}  // namespace dh
