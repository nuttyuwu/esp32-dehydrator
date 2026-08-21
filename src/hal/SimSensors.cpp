#include "hal/SimSensors.h"

#include <cstdio>

namespace dh {

Reading SimSensors::readBottom() {
    Reading r;
    if (fault_ == SensorFault::Bottom || fault_ == SensorFault::Both) return r;
    r.t = tBot_.set ? tBot_.v : plant_.tBot();
    r.rh = rhBot_.set ? rhBot_.v : plant_.rhBot();
    r.valid = true;
    return r;
}

Reading SimSensors::readTop() {
    Reading r;
    if (fault_ == SensorFault::Top || fault_ == SensorFault::Both) return r;
    r.t = tTop_.set ? tTop_.v : plant_.tTop();
    r.rh = rhTop_.set ? rhTop_.v : plant_.rhTop();
    r.valid = true;
    return r;
}

bool SimSensors::setOverride(const std::string& field, float value) {
    if (field == "t_bot") { tBot_ = {true, value}; return true; }
    if (field == "rh_bot") { rhBot_ = {true, value}; return true; }
    if (field == "t_top") { tTop_ = {true, value}; return true; }
    if (field == "rh_top") { rhTop_ = {true, value}; return true; }
    return false;
}

void SimSensors::clearOverrides() {
    tBot_ = Ovr{};
    rhBot_ = Ovr{};
    tTop_ = Ovr{};
    rhTop_ = Ovr{};
}

bool SimSensors::anyOverride() const {
    return tBot_.set || rhBot_.set || tTop_.set || rhTop_.set;
}

std::string SimSensors::overrideSummary() const {
    if (!anyOverride()) return "none";
    std::string s;
    char buf[32];
    auto add = [&](const char* name, const Ovr& o) {
        if (!o.set) return;
        if (!s.empty()) s += " ";
        std::snprintf(buf, sizeof(buf), "%s=%.1f", name, static_cast<double>(o.v));
        s += buf;
    };
    add("t_bot", tBot_);
    add("rh_bot", rhBot_);
    add("t_top", tTop_);
    add("rh_top", rhTop_);
    return s;
}

}  // namespace dh
