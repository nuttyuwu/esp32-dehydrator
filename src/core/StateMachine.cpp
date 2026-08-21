#include "core/StateMachine.h"

namespace dh {

void StateMachine::begin(uint32_t nowMs, bool watchdogReset) {
    ctrl_.begin(nowMs);
    safety_.begin(nowMs, watchdogReset);
    manual_ = ManualRequest{};
    session_ = Session{};
    lastHeaterCmd_ = false;
    rhBelow_ = false;
    mode_ = Mode::Off;
    enter(safety_.latched() ? State::Fault : State::Idle, nowMs);
}

void StateMachine::enter(State s, uint32_t nowMs) {
    if (state_ == s) return;
    state_ = s;
    stateSinceMs_ = nowMs;
    if (s != State::Drying) {
        rhBelow_ = false;
    }
}

void StateMachine::setMode(Mode m, uint32_t nowMs) {
    if (safety_.latched()) return;
    mode_ = m;
    if (m == Mode::Manual) {
        manualSinceMs_ = nowMs;
        enter(State::Manual, nowMs);
    } else if (m == Mode::Off) {
        session_.active = false;
        enter(State::Idle, nowMs);
    }
}

bool StateMachine::start(const std::string& label, uint32_t nowMs, uint32_t epoch,
                         uint32_t uptimeS) {
    if (safety_.latched()) return false;
    session_.id = nextId_++;
    session_.startedAtEpoch = epoch;
    session_.startedAtUptimeS = uptimeS;
    session_.active = true;
    session_.label = label;
    manual_ = ManualRequest{};
    mode_ = Mode::Auto;
    ctrl_.begin(nowMs);
    enter(State::Preheat, nowMs);
    return true;
}

void StateMachine::stop(uint32_t nowMs) {
    if (state_ == State::Preheat || state_ == State::Drying || state_ == State::Manual) {
        mode_ = Mode::Auto;
        enter(State::Cooldown, nowMs);
    } else {
        mode_ = Mode::Off;
        session_.active = false;
        enter(State::Idle, nowMs);
    }
}

void StateMachine::resetFault(uint32_t nowMs) {
    safety_.reset(nowMs);
    ctrl_.begin(nowMs);
    session_.active = false;
    mode_ = Mode::Off;
    state_ = State::Fault;   // force enter() to fire
    enter(State::Idle, nowMs);
}

void StateMachine::restoreSession(const Session& s, uint32_t nowMs) {
    session_ = s;
    if (s.id >= nextId_) nextId_ = s.id + 1;
    if (s.active && !safety_.latched()) {
        mode_ = Mode::Auto;
        enter(State::Preheat, nowMs);
    }
}

uint32_t StateMachine::elapsedS(uint32_t uptimeS) const {
    if (session_.id == 0) return 0;
    if (uptimeS < session_.startedAtUptimeS) return 0;
    return uptimeS - session_.startedAtUptimeS;
}

Outputs StateMachine::tick(const Config& cfg, SensorHub& hub, const TickInputs& in) {
    const uint32_t now = in.nowMs;
    const Reading ctrlR = hub.control();

    safety_.evaluate(hub, in.klixonOpen, lastHeaterCmd_, now);

    Outputs out;

    if (safety_.latched()) {
        ctrl_.forceOff(now);
        enter(State::Fault, now);
        session_.active = false;

        // Heater off, fans on to dump residual heat. If the sensors are the thing that
        // failed we cannot see the temperature, so purge blind for 10 minutes and stop.
        const bool hot = ctrlR.valid ? (ctrlR.t > limits::kCooldownExitC)
                                     : ((now - stateSinceMs_) < 600000u);
        out.heater = false;
        out.fanStack = hot;
        out.fanIntake = false;
        lastHeaterCmd_ = false;
        return out;
    }

    switch (mode_) {
        case Mode::Off: {
            enter(state_ == State::Done ? State::Done : State::Idle, now);
            ctrl_.update(cfg, ctrlR.t, ctrlR.valid, false, now);
            out.fanStack = ctrl_.inPurge(now);
            break;
        }

        case Mode::Manual: {
            if (now - manualSinceMs_ >= limits::kManualTimeoutMs) {
                mode_ = Mode::Off;
                manual_ = ManualRequest{};
                ctrl_.forceOff(now);
                enter(State::Idle, now);
                break;
            }
            enter(State::Manual, now);
            ctrl_.forceOff(now);   // the controller does not own the heater in manual
            out.heater = manual_.heater;
            out.fanIntake = manual_.fanIntake;
            out.fanStack = manual_.fanStack;
            break;
        }

        case Mode::Auto: {
            const State before = state_;
            switch (state_) {
                case State::Preheat:
                case State::Drying: {
                    ctrl_.update(cfg, ctrlR.t, ctrlR.valid, true, now);
                    out.heater = ctrl_.heater();
                    out.fanStack = true;

                    const bool solarUseful = in.collectorValid && ctrlR.valid &&
                                             (in.tCollector > ctrlR.t + limits::kIntakeDeltaC);
                    out.fanIntake = ctrl_.heater() || solarUseful || !in.collectorValid;

                    if (state_ == State::Preheat) {
                        if (ctrlR.valid && ctrlR.t >= cfg.setpointC - cfg.hysteresisC) {
                            enter(State::Drying, now);
                        }
                    } else {
                        const Reading& top = hub.top();
                        if (top.valid && top.rh < cfg.rhTargetPct) {
                            if (!rhBelow_) {
                                rhBelow_ = true;
                                rhBelowSinceMs_ = now;
                            } else if (now - rhBelowSinceMs_ >= limits::kFinishHoldMs) {
                                enter(State::Cooldown, now);
                            }
                        } else {
                            rhBelow_ = false;
                        }
                    }
                    break;
                }

                case State::Cooldown: {
                    ctrl_.update(cfg, ctrlR.t, ctrlR.valid, false, now);
                    out.heater = false;
                    out.fanStack = true;
                    out.fanIntake = true;
                    if (ctrlR.valid && ctrlR.t < limits::kCooldownExitC) {
                        session_.active = false;
                        enter(State::Done, now);
                    }
                    break;
                }

                case State::Done:
                case State::Idle:
                default: {
                    ctrl_.update(cfg, ctrlR.t, ctrlR.valid, false, now);
                    out.fanStack = ctrl_.inPurge(now);
                    break;
                }
            }

            // A transition decided inside the branch above happens *after* that branch has
            // already filled in the outputs, so the outputs would lag the state by one
            // tick — and the log row would show DRYING alongside a heater that should be
            // off. Re-derive them for the state we actually ended up in.
            if (state_ != before) {
                if (state_ == State::Cooldown) {
                    ctrl_.forceOff(now);
                    out.heater = false;
                    out.fanStack = true;
                    out.fanIntake = true;
                } else if (state_ == State::Done) {
                    ctrl_.forceOff(now);
                    out = Outputs{};
                }
            }
            break;
        }
    }

    // The heater is meaningless without the stack fan; the actuator layer enforces this
    // too, but keeping the request self-consistent makes the logs readable.
    if (out.heater && !out.fanStack) out.heater = false;

    lastHeaterCmd_ = out.heater;
    return out;
}

}  // namespace dh
