#include "core/Metrics.h"

#include <cmath>

namespace dh {
namespace metrics {

float psatKPa(float tC) {
    return 0.61078f * std::exp(17.27f * tC / (tC + 237.3f));
}

float absHumidity(float tC, float rhPct) {
    if (rhPct < 0.0f) rhPct = 0.0f;
    if (rhPct > 100.0f) rhPct = 100.0f;
    const float pvHpa = (rhPct / 100.0f) * psatKPa(tC) * 10.0f;   // kPa -> hPa
    return 216.7f * pvHpa / (273.15f + tC);
}

float vpdKPa(float tC, float rhPct) {
    if (rhPct < 0.0f) rhPct = 0.0f;
    if (rhPct > 100.0f) rhPct = 100.0f;
    return psatKPa(tC) * (1.0f - rhPct / 100.0f);
}

float dewPointC(float tC, float rhPct) {
    if (rhPct < 1.0f) rhPct = 1.0f;
    if (rhPct > 100.0f) rhPct = 100.0f;
    const float a = 17.27f, b = 237.3f;
    const float g = (a * tC) / (b + tC) + std::log(rhPct / 100.0f);
    return (b * g) / (a - g);
}

}  // namespace metrics
}  // namespace dh
