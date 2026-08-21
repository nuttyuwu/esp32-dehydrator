#include "core/Safety.h"

namespace dh {

void Safety::begin(uint32_t nowMs, bool watchdogReset) {
    latched_ = false;
    fault_ = Fault::None;
    heaterArmed_ = false;
    heaterOnSinceMs_ = nowMs;
    tAtHeaterOnValid_ = false;
    if (watchdogReset) {
        // Came back from a watchdog or brownout with unknown output state. Refuse to heat
        // until a human has looked at it.
        trip(Fault::WatchdogReset);
    }
}

void Safety::reset(uint32_t nowMs) {
    latched_ = false;
    fault_ = Fault::None;
    heaterArmed_ = false;
    heaterOnSinceMs_ = nowMs;
    tAtHeaterOnValid_ = false;
}

void Safety::trip(Fault f) {
    if (latched_) return;   // keep the first cause, it is the useful one
    latched_ = true;
    fault_ = f;
}

Fault Safety::evaluate(const SensorHub& hub, bool klixonOpen, bool heaterCommanded,
                       uint32_t nowMs) {
    // Unfiltered: a protective limit must not wait for an EMA to catch up. At alpha 0.3 a
    // jump to 70 C would take six ticks to cross the 65 C line.
    const Reading& bot = hub.rawBottom();
    const Reading& top = hub.rawTop();

    if (klixonOpen) trip(Fault::Klixon);

    if ((bot.valid && bot.t >= limits::kOverTempC) || (top.valid && top.t >= limits::kOverTempC)) {
        trip(Fault::OverTemp);
    }

    if (hub.invalidForMs(nowMs) > limits::kSensorTimeoutMs) {
        trip(Fault::SensorTimeout);
    }

    // Heater effectiveness.
    const Reading ctrl = hub.control();
    if (heaterCommanded) {
        if (!heaterArmed_) {
            heaterArmed_ = true;
            heaterOnSinceMs_ = nowMs;
            tAtHeaterOn_ = ctrl.t;
            tAtHeaterOnValid_ = ctrl.valid;
        } else if (!tAtHeaterOnValid_ && ctrl.valid) {
            // Sensor came good after the heater started; take the baseline from here.
            heaterOnSinceMs_ = nowMs;
            tAtHeaterOn_ = ctrl.t;
            tAtHeaterOnValid_ = true;
        } else if (tAtHeaterOnValid_ && ctrl.valid &&
                   (nowMs - heaterOnSinceMs_) >= limits::kHeaterCheckMs) {
            if ((ctrl.t - tAtHeaterOn_) < limits::kHeaterRiseMinC) {
                trip(Fault::HeaterIneffective);
            } else {
                // Rising fine. Re-arm from here so a later stall is still caught.
                heaterOnSinceMs_ = nowMs;
                tAtHeaterOn_ = ctrl.t;
            }
        }
    } else {
        heaterArmed_ = false;
        tAtHeaterOnValid_ = false;
    }

    return fault_;
}

}  // namespace dh
