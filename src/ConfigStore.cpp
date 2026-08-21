#include "ConfigStore.h"

#include <Arduino.h>
#include <Preferences.h>

namespace dh {

namespace {
constexpr const char* kNsCfg = "dh-cfg";
constexpr const char* kNsSes = "dh-ses";
}  // namespace

bool ConfigStore::begin() {
    // Create both namespaces up front. Preferences logs a scary ERROR line every time a
    // read-only open or a missing key is hit, and on a virgin board that is a whole screen
    // of NOT_FOUND noise at boot that looks like a failure and is not.
    Preferences p;
    ok_ = p.begin(kNsCfg, false);
    if (ok_) p.end();

    Preferences s;
    if (s.begin(kNsSes, false)) s.end();

    return ok_;
}

void ConfigStore::load(Config& cfg) {
    Preferences p;
    if (!p.begin(kNsCfg, true)) return;

    // isKey() first, so an unset key keeps the compiled-in default silently.
    if (p.isKey("setpoint")) cfg.setpointC = p.getFloat("setpoint", cfg.setpointC);
    if (p.isKey("hyst")) cfg.hysteresisC = p.getFloat("hyst", cfg.hysteresisC);
    if (p.isKey("rhtgt")) cfg.rhTargetPct = p.getFloat("rhtgt", cfg.rhTargetPct);
    if (p.isKey("logint")) cfg.logIntervalS = p.getUInt("logint", cfg.logIntervalS);
    if (p.isKey("minon")) cfg.minOnS = p.getUInt("minon", cfg.minOnS);
    if (p.isKey("minoff")) cfg.minOffS = p.getUInt("minoff", cfg.minOffS);
    if (p.isKey("obt")) cfg.offBotT = p.getFloat("obt", cfg.offBotT);
    if (p.isKey("obrh")) cfg.offBotRh = p.getFloat("obrh", cfg.offBotRh);
    if (p.isKey("ott")) cfg.offTopT = p.getFloat("ott", cfg.offTopT);
    if (p.isKey("otrh")) cfg.offTopRh = p.getFloat("otrh", cfg.offTopRh);
    if (p.isKey("tz")) cfg.tzOffsetMin = p.getInt("tz", cfg.tzOffsetMin);

    if (p.isKey("ssid")) {
        String ssid = p.getString("ssid", cfg.apSsid);
        snprintf(cfg.apSsid, sizeof(cfg.apSsid), "%s", ssid.c_str());
    }
    if (p.isKey("pass")) {
        String pass = p.getString("pass", cfg.apPass);
        snprintf(cfg.apPass, sizeof(cfg.apPass), "%s", pass.c_str());
    }

    p.end();
}

void ConfigStore::save(const Config& cfg) {
    Preferences p;
    if (!p.begin(kNsCfg, false)) return;

    p.putFloat("setpoint", cfg.setpointC);
    p.putFloat("hyst", cfg.hysteresisC);
    p.putFloat("rhtgt", cfg.rhTargetPct);
    p.putUInt("logint", cfg.logIntervalS);
    p.putUInt("minon", cfg.minOnS);
    p.putUInt("minoff", cfg.minOffS);
    p.putFloat("obt", cfg.offBotT);
    p.putFloat("obrh", cfg.offBotRh);
    p.putFloat("ott", cfg.offTopT);
    p.putFloat("otrh", cfg.offTopRh);
    p.putInt("tz", cfg.tzOffsetMin);
    p.putString("ssid", cfg.apSsid);
    p.putString("pass", cfg.apPass);

    p.end();
}

void ConfigStore::loadSession(Session& s) {
    Preferences p;
    if (!p.begin(kNsSes, true)) return;
    if (p.isKey("id")) s.id = p.getUInt("id", 0);
    if (p.isKey("epoch")) s.startedAtEpoch = p.getUInt("epoch", 0);
    s.startedAtUptimeS = 0;   // a reboot resets uptime; elapsed restarts from here
    if (p.isKey("active")) s.active = p.getBool("active", false);
    if (p.isKey("label")) s.label = std::string(p.getString("label", "").c_str());
    p.end();
}

void ConfigStore::saveSession(const Session& s) {
    Preferences p;
    if (!p.begin(kNsSes, false)) return;
    p.putUInt("id", s.id);
    p.putUInt("epoch", s.startedAtEpoch);
    p.putBool("active", s.active);
    p.putString("label", s.label.c_str());
    p.end();
}

void ConfigStore::clearSession() {
    Preferences p;
    if (!p.begin(kNsSes, false)) return;
    p.putBool("active", false);
    p.end();
}

}  // namespace dh
