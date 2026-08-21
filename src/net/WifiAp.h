#pragma once

#include <DNSServer.h>
#include <IPAddress.h>
#include <WiFi.h>

#include "core/Types.h"

namespace dh {

class WifiAp {
  public:
    bool begin(const Config& cfg);
    void loop();   // pump the captive-portal DNS

    const String& ssid() const { return ssid_; }
    IPAddress ip() const { return WiFi.softAPIP(); }
    int clients() const { return WiFi.softAPgetStationNum(); }

  private:
    DNSServer dns_;
    String ssid_;
    bool up_ = false;
};

}  // namespace dh
