#include "core/Types.h"

namespace dh {

const char* stateName(State s) {
    switch (s) {
        case State::Boot: return "BOOT";
        case State::Idle: return "IDLE";
        case State::Preheat: return "PREHEAT";
        case State::Drying: return "DRYING";
        case State::Cooldown: return "COOLDOWN";
        case State::Done: return "DONE";
        case State::Manual: return "MANUAL";
        case State::Fault: return "FAULT";
    }
    return "?";
}

const char* modeName(Mode m) {
    switch (m) {
        case Mode::Off: return "off";
        case Mode::Auto: return "auto";
        case Mode::Manual: return "manual";
    }
    return "?";
}

const char* faultName(Fault f) {
    switch (f) {
        case Fault::None: return "none";
        case Fault::OverTemp: return "overtemp";
        case Fault::SensorTimeout: return "sensor_timeout";
        case Fault::HeaterIneffective: return "heater_ineffective";
        case Fault::Klixon: return "klixon";
        case Fault::WatchdogReset: return "watchdog_reset";
    }
    return "?";
}

bool parseMode(const std::string& s, Mode& out) {
    if (s == "off") { out = Mode::Off; return true; }
    if (s == "auto") { out = Mode::Auto; return true; }
    if (s == "manual") { out = Mode::Manual; return true; }
    return false;
}

}  // namespace dh
