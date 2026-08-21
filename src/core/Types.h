// Shared value types and tuning constants.
// Pure C++17 — no Arduino, no FreeRTOS. Compiles on the host for unit tests.
#pragma once

#include <cstdint>
#include <string>

namespace dh {

struct Reading {
    float t = 0.0f;    // degC
    float rh = 0.0f;   // %
    bool valid = false;
};

struct Outputs {
    bool heater = false;
    bool fanIntake = false;   // the 2 collector intake fans, driven as one group
    bool fanStack = false;    // the up-flow fan
};

inline bool operator==(const Outputs& a, const Outputs& b) {
    return a.heater == b.heater && a.fanIntake == b.fanIntake && a.fanStack == b.fanStack;
}
inline bool operator!=(const Outputs& a, const Outputs& b) { return !(a == b); }

enum class Mode : uint8_t { Off = 0, Auto, Manual };

enum class State : uint8_t { Boot = 0, Idle, Preheat, Drying, Cooldown, Done, Manual, Fault };

enum class Fault : uint8_t {
    None = 0,
    OverTemp,
    SensorTimeout,
    HeaterIneffective,
    Klixon,
    WatchdogReset,
};

const char* stateName(State s);
const char* modeName(Mode m);
const char* faultName(Fault f);
bool parseMode(const std::string& s, Mode& out);

namespace limits {
constexpr float kOverTempC = 65.0f;          // latched trip on either sensor
constexpr float kCooldownExitC = 30.0f;      // COOLDOWN -> DONE
constexpr float kHeaterRiseMinC = 1.0f;      // expected rise within kHeaterCheckMs
constexpr float kIntakeDeltaC = 2.0f;        // collector must beat cabinet by this
constexpr uint32_t kSensorTimeoutMs = 10000;
constexpr uint32_t kHeaterCheckMs = 600000;  // 10 min
constexpr uint32_t kPurgeMs = 60000;         // stack-fan post-purge
constexpr uint32_t kFinishHoldMs = 900000;   // 15 min below rh_target
constexpr uint32_t kManualTimeoutMs = 1800000;
}  // namespace limits

struct Config {
    float setpointC = 44.0f;
    float hysteresisC = 1.0f;    // ON at setpoint-h/2, OFF at setpoint+h/2
    float rhTargetPct = 25.0f;
    uint32_t logIntervalS = 10;
    uint32_t minOnS = 30;
    uint32_t minOffS = 30;
    float offBotT = 0.0f;
    float offBotRh = 0.0f;
    float offTopT = 0.0f;
    float offTopRh = 0.0f;
    int32_t tzOffsetMin = 480;   // Ulaanbaatar, UTC+8 — only affects log file naming
    char apSsid[33] = "DEHYDRATOR";
    // 10 chars — WPA2 needs at least 8, or WifiAp falls back to an open network.
    char apPass[33] = "[REDACTED]";
};

struct Session {
    uint32_t id = 0;
    uint32_t startedAtEpoch = 0;
    uint32_t startedAtUptimeS = 0;
    bool active = false;
    std::string label;
};

// One immutable copy of everything a reader needs. Produced under the state mutex.
struct Snapshot {
    float tBot = 0.0f, rhBot = 0.0f, tTop = 0.0f, rhTop = 0.0f;
    float ahBot = 0.0f, ahTop = 0.0f, vpdTop = 0.0f;
    bool botValid = false, topValid = false;
    Outputs out;
    State state = State::Boot;
    Mode mode = Mode::Off;
    Fault fault = Fault::None;
    bool faultLatched = false;
    Session session;
    uint32_t elapsedS = 0;
    uint32_t uptimeS = 0;
    uint32_t epoch = 0;
    bool timeKnown = false;
    float setpointC = 44.0f;
    float moisture = -1.0f;   // sim build only, -1 on hw
    float simSpeed = 1.0f;    // sim build only
};

}  // namespace dh
