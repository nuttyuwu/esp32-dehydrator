#include "net/WifiAp.h"

#include <Arduino.h>

namespace dh {

bool WifiAp::begin(const Config& cfg) {
    // Order matters: softAPmacAddress() returns all zeros until the Wi-Fi driver is up, and
    // every unit would then advertise the same "-0000" SSID.
    WiFi.mode(WIFI_AP);

    uint8_t mac[6] = {0};
    WiFi.softAPmacAddress(mac);

    char suffix[8];
    snprintf(suffix, sizeof(suffix), "-%02X%02X", mac[4], mac[5]);
    ssid_ = String(cfg.apSsid) + suffix;

    // WPA2 needs 8 characters. A shorter password would silently create an open network,
    // which is worse than refusing, so fall back explicitly and say so.
    const bool secured = strlen(cfg.apPass) >= 8;
    if (!secured) {
        Serial.println("[wifi] AP password shorter than 8 chars — starting an OPEN network");
    }
    up_ = WiFi.softAP(ssid_.c_str(), secured ? cfg.apPass : nullptr);
    if (!up_) {
        Serial.println("[wifi] softAP failed");
        return false;
    }

    dns_.setErrorReplyCode(DNSReplyCode::NoError);
    dns_.start(53, "*", WiFi.softAPIP());

    Serial.printf("[wifi] AP \"%s\" at %s\n", ssid_.c_str(), WiFi.softAPIP().toString().c_str());
    return true;
}

void WifiAp::loop() {
    if (up_) dns_.processNextRequest();
}

}  // namespace dh
