#include "core/Controller.h"

namespace dh {

void Controller::begin(uint32_t nowMs) {
    heater_ = false;
    lastOnMs_ = nowMs;
    lastOffMs_ = nowMs;
    everOff_ = false;   // no dwell constraint before the first cycle
}

void Controller::forceOff(uint32_t nowMs) {
    if (heater_) {
        heater_ = false;
        lastOffMs_ = nowMs;
        everOff_ = true;
    }
}

bool Controller::inPurge(uint32_t nowMs) const {
    if (heater_ || !everOff_) return false;
    return (nowMs - lastOffMs_) < limits::kPurgeMs;
}

bool Controller::update(const Config& cfg, float tCtrl, bool tValid, bool allowHeat,
                        uint32_t nowMs) {
    if (!allowHeat || !tValid) {
        forceOff(nowMs);
        return heater_;
    }

    const uint32_t minOnMs = cfg.minOnS * 1000u;
    const uint32_t minOffMs = cfg.minOffS * 1000u;

    if (heater_) {
        const bool dwellOk = (nowMs - lastOnMs_) >= minOnMs;
        if (tCtrl >= offThreshold(cfg) && dwellOk) {
            heater_ = false;
            lastOffMs_ = nowMs;
            everOff_ = true;
        }
    } else {
        const bool dwellOk = !everOff_ || (nowMs - lastOffMs_) >= minOffMs;
        if (tCtrl <= onThreshold(cfg) && dwellOk) {
            heater_ = true;
            lastOnMs_ = nowMs;
        }
    }
    return heater_;
}

}  // namespace dh
