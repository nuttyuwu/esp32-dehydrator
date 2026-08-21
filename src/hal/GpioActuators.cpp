#include "hal/GpioActuators.h"

#include <Arduino.h>

#include "hal/Pins.h"

namespace dh {

void GpioActuators::driveRelay(int pin, bool on) {
    const bool level = pins::kRelayActiveLow ? !on : on;
    digitalWrite(pin, level ? HIGH : LOW);
}

bool GpioActuators::begin() {
    // Set the idle level *before* switching the pin to an output, so the relay never sees a
    // glancing pulse on boot.
    const int idle = pins::kRelayActiveLow ? HIGH : LOW;
    for (int pin : {pins::kHeater, pins::kFanIntakeA, pins::kFanIntakeB, pins::kFanStack}) {
        digitalWrite(pin, idle);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, idle);
    }
    pinMode(pins::kKlixon, INPUT);   // GPIO34 has no internal pull-up: fit an external one
    cur_ = Outputs{};
    return true;
}

bool GpioActuators::klixonOpen() {
    const bool high = digitalRead(pins::kKlixon) == HIGH;
    return pins::kKlixonOpenIsHigh ? high : !high;
}

void GpioActuators::write(const Outputs& o) {
    driveRelay(pins::kHeater, o.heater);
    driveRelay(pins::kFanIntakeA, o.fanIntake);
    driveRelay(pins::kFanIntakeB, o.fanIntake);
    driveRelay(pins::kFanStack, o.fanStack);
}

}  // namespace dh
