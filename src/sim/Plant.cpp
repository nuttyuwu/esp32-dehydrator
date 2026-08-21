#include "sim/Plant.h"

#include <algorithm>

#include "core/Metrics.h"

namespace dh {
namespace sim {

namespace {
constexpr float kMaxSubStepS = 5.0f;   // explicit Euler goes unstable past this at speed 500

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
}  // namespace

void Plant::reset() {
    tCab_ = tAmb_;
    m_ = 1.0f;
    out_ = Outputs{};
}

void Plant::step(float dtS) {
    float remaining = dtS * speed_;
    while (remaining > 0.0f) {
        const float h = std::min(kMaxSubStepS, remaining);
        remaining -= h;

        const float heat = out_.heater ? p_.kHeater : 0.0f;
        const float solarGain = out_.fanIntake ? p_.kSolar * solar_ : 0.0f;
        const float loss = (p_.kLoss + (out_.fanStack ? p_.kFanLoss : 0.0f)) * (tCab_ - tAmb_);
        tCab_ += (heat + solarGain - loss) * h;

        const float driving = std::max(0.0f, tCab_ - tAmb_);
        const float airflow = 0.5f + (out_.fanStack ? 0.5f : 0.0f);
        m_ -= p_.cDry * m_ * driving * airflow * h;
        if (m_ < 0.0f) m_ = 0.0f;
    }
}

float Plant::tBot() const { return tCab_; }

float Plant::rhBot() const {
    // Heating the intake air without adding water drops its relative humidity.
    const float rh = rhAmb_ * metrics::psatKPa(tAmb_) / metrics::psatKPa(tCab_);
    return clampf(rh, 1.0f, 99.0f);
}

float Plant::tTop() const {
    const float stratification = 2.0f + (out_.fanStack ? 0.0f : 2.0f);
    return tCab_ - stratification - 3.0f * m_;   // last term is evaporative cooling
}

float Plant::rhTop() const { return clampf(rhBot() + 70.0f * m_, 1.0f, 99.0f); }

}  // namespace sim
}  // namespace dh
