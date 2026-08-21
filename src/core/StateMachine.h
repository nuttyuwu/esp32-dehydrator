// Session lifecycle and output arbitration. ARCHITECTURE.md section 7.
//
//   BOOT -> IDLE -> PREHEAT -> DRYING -> COOLDOWN -> DONE -> IDLE
//                                  \                  /
//                                   +-> FAULT (latched, manual reset)
#pragma once

#include <cstdint>
#include <string>

#include "core/Controller.h"
#include "core/Safety.h"
#include "core/SensorHub.h"
#include "core/Types.h"

namespace dh {

struct ManualRequest {
    bool heater = false;
    bool fanIntake = false;
    bool fanStack = false;
};

// Optional collector-outlet temperature. There is no collector sensor in the v1 pin map,
// so collectorValid is normally false and the intake fans fall back to "run while heating"
// (documented in README.md).
struct TickInputs {
    uint32_t nowMs = 0;
    bool klixonOpen = false;
    float tCollector = 0.0f;
    bool collectorValid = false;
};

class StateMachine {
  public:
    void begin(uint32_t nowMs, bool watchdogReset);

    // Commands. All are safe to call from any task as long as the caller holds the
    // command mutex in main.cpp.
    void setMode(Mode m, uint32_t nowMs);
    bool start(const std::string& label, uint32_t nowMs, uint32_t epoch, uint32_t uptimeS);
    void stop(uint32_t nowMs);
    void resetFault(uint32_t nowMs);
    void setManual(const ManualRequest& r) { manual_ = r; }
    void restoreSession(const Session& s, uint32_t nowMs);

    // One 1 Hz control tick. Returns the outputs to hand to IActuators::apply().
    Outputs tick(const Config& cfg, SensorHub& hub, const TickInputs& in);

    State state() const { return state_; }
    Mode mode() const { return mode_; }
    Fault fault() const { return safety_.fault(); }
    bool faultLatched() const { return safety_.latched(); }
    const Session& session() const { return session_; }
    const ManualRequest& manual() const { return manual_; }
    uint32_t elapsedS(uint32_t uptimeS) const;
    uint32_t nextSessionId() const { return nextId_; }
    void setNextSessionId(uint32_t id) { nextId_ = id; }

  private:
    void enter(State s, uint32_t nowMs);

    State state_ = State::Boot;
    Mode mode_ = Mode::Off;
    Controller ctrl_;
    Safety safety_;
    Session session_;
    ManualRequest manual_;

    uint32_t stateSinceMs_ = 0;
    uint32_t manualSinceMs_ = 0;
    uint32_t rhBelowSinceMs_ = 0;
    bool rhBelow_ = false;
    uint32_t nextId_ = 1;
    // What we actually commanded last tick — including in MANUAL, where the controller
    // is not the one holding the heater. Safety's effectiveness check reads this.
    bool lastHeaterCmd_ = false;
};

}  // namespace dh
