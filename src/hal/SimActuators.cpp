#include "hal/SimActuators.h"

#include <Arduino.h>

namespace dh {

void SimActuators::onChange(const Outputs& o) {
    Serial.printf("[out] heater=%d fan_in=%d fan_up=%d\n", o.heater ? 1 : 0, o.fanIntake ? 1 : 0,
                  o.fanStack ? 1 : 0);
}

}  // namespace dh
