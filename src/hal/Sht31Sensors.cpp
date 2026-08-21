#include "hal/Sht31Sensors.h"

#include <Arduino.h>
#include <Wire.h>

#include <cmath>

#include "hal/Pins.h"

namespace dh {

bool Sht31Sensors::begin() {
    Wire.begin(pins::kI2cSda, pins::kI2cScl);
    Wire.setClock(100000);   // 100 kHz — the run up to the top sensor is long

    botOk_ = bot_.begin(pins::kSht31AddrBottom);
    topOk_ = top_.begin(pins::kSht31AddrTop);

    Serial.printf("[sht31] bottom 0x%02X %s, top 0x%02X %s\n", pins::kSht31AddrBottom,
                  botOk_ ? "ok" : "MISSING", pins::kSht31AddrTop, topOk_ ? "ok" : "MISSING");
    return botOk_ || topOk_;
}

Reading Sht31Sensors::read(Adafruit_SHT31& dev, bool present) {
    Reading r;
    if (!present) return r;

    const float t = dev.readTemperature();
    const float rh = dev.readHumidity();
    if (std::isnan(t) || std::isnan(rh)) return r;
    if (t < -40.0f || t > 125.0f || rh < 0.0f || rh > 100.0f) return r;

    r.t = t;
    r.rh = rh;
    r.valid = true;
    return r;
}

Reading Sht31Sensors::readBottom() {
    Reading r = read(bot_, botOk_);
    if (!r.valid && botOk_) {
        // One retry, then let SensorHub's 10 s timeout decide it is a real fault.
        r = read(bot_, botOk_);
        if (!r.valid) botOk_ = bot_.begin(pins::kSht31AddrBottom);
    }
    return r;
}

Reading Sht31Sensors::readTop() {
    Reading r = read(top_, topOk_);
    if (!r.valid && topOk_) {
        r = read(top_, topOk_);
        if (!r.valid) topOk_ = top_.begin(pins::kSht31AddrTop);
    }
    return r;
}

}  // namespace dh
