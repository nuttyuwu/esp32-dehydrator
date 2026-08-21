// Simulated sensors: read the plant model, with TUI overrides and fault injection on top.
// Overrides shadow the model output without stopping the model, so "sim clear" returns to a
// plausible state rather than a frozen one.
#pragma once

#include "hal/ISensors.h"
#include "sim/Plant.h"

namespace dh {

enum class SensorFault : uint8_t { None = 0, Bottom, Top, Both };

class SimSensors : public ISensors {
  public:
    explicit SimSensors(sim::Plant& plant) : plant_(plant) {}

    bool begin() override { return true; }
    Reading readBottom() override;
    Reading readTop() override;

    // field is one of: t_bot, rh_bot, t_top, rh_top
    bool setOverride(const std::string& field, float value);
    void clearOverrides();
    bool anyOverride() const;
    std::string overrideSummary() const;

    void setFault(SensorFault f) { fault_ = f; }
    SensorFault fault() const { return fault_; }

  private:
    struct Ovr {
        bool set = false;
        float v = 0.0f;
    };

    sim::Plant& plant_;
    Ovr tBot_, rhBot_, tTop_, rhTop_;
    SensorFault fault_ = SensorFault::None;
};

}  // namespace dh
