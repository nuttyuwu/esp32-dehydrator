// Psychrometrics. ARCHITECTURE.md section 9.
#pragma once

namespace dh {
namespace metrics {

// Saturation vapour pressure, Magnus formula. Returns kPa.
float psatKPa(float tC);

// Absolute humidity in g/m3.
float absHumidity(float tC, float rhPct);

// Vapour pressure deficit in kPa.
float vpdKPa(float tC, float rhPct);

// Dew point in degC. Handy for diagnosing condensation on the glass.
float dewPointC(float tC, float rhPct);

}  // namespace metrics
}  // namespace dh
