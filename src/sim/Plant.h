// Thermal + moisture model of the cabinet. ARCHITECTURE.md section 4.
//
// Pure C++17 so the host tests can drive it. Constants are tuned so that heater-on steady
// state lands at ~46 C — just above the 44 C setpoint, which is what makes the hysteresis
// genuinely cycle instead of pinning on — with a ~3 min thermal time constant and a ~4 h
// drying curve. Fresh product reads ~80 %RH at the top sensor, matching the field data.
#pragma once

#include "core/Types.h"

namespace dh {
namespace sim {

struct PlantParams {
    float kHeater = 0.12f;     // degC/s at full heater
    float kSolar = 0.03f;      // degC/s at full sun, intake fan running
    float kLoss = 0.0035f;     // 1/s to ambient
    float kFanLoss = 0.0015f;  // extra 1/s while the stack fan runs
    float cDry = 3.0e-6f;      // moisture decay coefficient
};

class Plant {
  public:
    void reset();
    void setOutputs(const Outputs& o) { out_ = o; }
    void step(float dtS);

    // Model state
    float tCab() const { return tCab_; }
    float moisture() const { return m_; }

    // What the sensors would read
    float tBot() const;
    float rhBot() const;
    float tTop() const;
    float rhTop() const;

    // Knobs the TUI turns
    void setAmbient(float t) { tAmb_ = t; }
    void setAmbientRh(float rh) { rhAmb_ = rh; }
    void setSolar(float s) { solar_ = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s); }
    void setMoisture(float m) { m_ = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m); }
    void setSpeed(float f) { speed_ = f < 1.0f ? 1.0f : (f > 500.0f ? 500.0f : f); }
    void setCabTemp(float t) { tCab_ = t; }

    float ambient() const { return tAmb_; }
    float ambientRh() const { return rhAmb_; }
    float solar() const { return solar_; }
    float speed() const { return speed_; }
    PlantParams& params() { return p_; }

  private:
    PlantParams p_;
    Outputs out_;
    float tCab_ = 22.0f;
    float m_ = 1.0f;
    float tAmb_ = 22.0f;
    float rhAmb_ = 40.0f;
    float solar_ = 0.0f;
    float speed_ = 1.0f;
};

}  // namespace sim
}  // namespace dh
