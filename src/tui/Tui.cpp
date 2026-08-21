#include "tui/Tui.h"

#include <Arduino.h>
#include <LittleFS.h>

#include <cstdlib>
#include <deque>

#include "App.h"

namespace dh {

namespace {

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && s[i] == ' ') i++;
        size_t j = i;
        while (j < s.size() && s[j] != ' ') j++;
        if (j > i) out.push_back(s.substr(i, j - i));
        i = j;
    }
    return out;
}

float toF(const std::string& s) { return static_cast<float>(atof(s.c_str())); }
bool onOff(const std::string& s) { return s == "on" || s == "1" || s == "true"; }

const char* yn(bool b) { return b ? "on" : "off"; }

// void-returning wrappers so the handlers can `return say(...)` on a usage error.
void say(const char* s) { Serial.println(s); }

template <typename... A>
void sayf(const char* fmt, A... args) {
    Serial.printf(fmt, args...);
}

}  // namespace

void Tui::begin(WifiAp* wifi) {
    wifi_ = wifi;
    Serial.println();
    Serial.println("dehydrator sim console — type 'help'");
}

void Tui::poll() {
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') continue;
        if (c == '\n') {
            const std::string line = buf_;
            buf_.clear();
            dispatch(line);
        } else if (buf_.size() < 120) {
            buf_ += c;
        }
    }

    if (mon_ && (millis() - lastMonMs_) >= 1000) {
        lastMonMs_ = millis();
        printMonLine();
    }
}

void Tui::dispatch(const std::string& line) {
    const auto a = split(line);
    if (a.empty()) return;
    const std::string& cmd = a[0];

    if (cmd == "help") return printHelp();
    if (cmd == "show") return printShow();

    if (cmd == "mon") {
        if (a.size() < 2) return say("mon on|off");
        mon_ = onOff(a[1]);
        Serial.printf("mon %s\n", yn(mon_));
        return;
    }

    if (cmd == "set") return handleSet(a);
    if (cmd == "sim") return handleSim(a);
    if (cmd == "fault") return handleFault(a);
    if (cmd == "manual") return handleManual(a);
    if (cmd == "log") return handleLog(a);

    if (cmd == "save") {
        app.cmdSaveConfig();
        Serial.println("config saved to NVS");
        return;
    }

    if (cmd == "mode") {
        Mode m;
        if (a.size() < 2 || !parseMode(a[1], m)) return say("mode off|auto|manual");
        if (!app.cmdMode(m)) return say("refused: fault latched, run 'reset'");
        Serial.printf("mode %s\n", modeName(m));
        return;
    }

    if (cmd == "start") {
        std::string label;
        for (size_t i = 1; i < a.size(); i++) {
            if (!label.empty()) label += " ";
            label += a[i];
        }
        if (!app.cmdStart(label)) return say("refused: fault latched, run 'reset'");
        Serial.printf("session %u started: %s\n", app.sm.session().id,
                      label.empty() ? "(no label)" : label.c_str());
        return;
    }

    if (cmd == "stop") {
        app.cmdStop();
        Serial.println("stopping — cooldown");
        return;
    }

    if (cmd == "reset") {
        app.cmdResetFault();
        Serial.println("fault cleared");
        return;
    }

    if (cmd == "wifi") {
        if (!wifi_) return say("wifi not started");
        Serial.printf("ssid=%s ip=%s clients=%d\n", wifi_->ssid().c_str(),
                      wifi_->ip().toString().c_str(), wifi_->clients());
        return;
    }

    Serial.printf("unknown command '%s' — try 'help'\n", cmd.c_str());
}

void Tui::handleSet(const std::vector<std::string>& a) {
    if (a.size() < 3) return say("set setpoint|hyst|rh_target|loginterval <value>");
    const std::string& k = a[1];
    const float v = toF(a[2]);

    if (k == "setpoint") app.cfg.setpointC = v;
    else if (k == "hyst") app.cfg.hysteresisC = v;
    else if (k == "rh_target") app.cfg.rhTargetPct = v;
    else if (k == "loginterval") app.cfg.logIntervalS = static_cast<uint32_t>(v);
    else if (k == "minon") app.cfg.minOnS = static_cast<uint32_t>(v);
    else if (k == "minoff") app.cfg.minOffS = static_cast<uint32_t>(v);
    else return sayf("no such setting '%s'\n", k.c_str());

    Serial.printf("%s = %.2f  (not persisted until 'save')\n", k.c_str(),
                  static_cast<double>(v));
}

void Tui::handleSim(const std::vector<std::string>& a) {
    if (!app.plant || !app.simSensors) return say("sim not available in this build");
    if (a.size() < 2) return say("sim amb|solar|moisture|speed|t_bot|rh_bot|t_top|rh_top <v> | sim clear");

    const std::string& k = a[1];
    if (k == "clear") {
        app.simSensors->clearOverrides();
        Serial.println("overrides cleared");
        return;
    }
    if (a.size() < 3) return sayf("sim %s <value>\n", k.c_str());
    const float v = toF(a[2]);

    if (k == "amb") { app.plant->setAmbient(v); Serial.printf("ambient = %.1f C\n", static_cast<double>(v)); return; }
    if (k == "ambrh") { app.plant->setAmbientRh(v); Serial.printf("ambient rh = %.1f %%\n", static_cast<double>(v)); return; }
    if (k == "solar") { app.plant->setSolar(v); Serial.printf("solar = %.2f\n", static_cast<double>(app.plant->solar())); return; }
    if (k == "moisture") { app.plant->setMoisture(v); Serial.printf("moisture = %.2f\n", static_cast<double>(app.plant->moisture())); return; }
    if (k == "speed") { app.plant->setSpeed(v); Serial.printf("time x%.0f\n", static_cast<double>(app.plant->speed())); return; }
    if (k == "tcab") { app.plant->setCabTemp(v); Serial.printf("cabinet = %.1f C\n", static_cast<double>(v)); return; }

    if (app.simSensors->setOverride(k, v)) {
        Serial.printf("override %s = %.1f  ('sim clear' to drop)\n", k.c_str(),
                      static_cast<double>(v));
        return;
    }
    Serial.printf("no such sim knob '%s'\n", k.c_str());
}

void Tui::handleFault(const std::vector<std::string>& a) {
    if (a.size() < 3) return say("fault sensor bot|top|both|none | fault klixon on|off");

    if (a[1] == "sensor") {
        if (!app.simSensors) return say("sim not available in this build");
        SensorFault f = SensorFault::None;
        if (a[2] == "bot") f = SensorFault::Bottom;
        else if (a[2] == "top") f = SensorFault::Top;
        else if (a[2] == "both") f = SensorFault::Both;
        else if (a[2] != "none") return say("fault sensor bot|top|both|none");
        app.simSensors->setFault(f);
        Serial.printf("sensor fault: %s\n", a[2].c_str());
        return;
    }

    if (a[1] == "klixon") {
        if (!app.simActuators) return say("sim not available in this build");
        app.simActuators->setKlixon(onOff(a[2]));
        Serial.printf("klixon open: %s\n", yn(onOff(a[2])));
        return;
    }

    Serial.println("fault sensor ... | fault klixon ...");
}

void Tui::handleManual(const std::vector<std::string>& a) {
    if (a.size() < 3) return say("manual heater|fan_in|fan_up on|off");

    ManualRequest r = app.sm.manual();
    const bool v = onOff(a[2]);
    if (a[1] == "heater") r.heater = v;
    else if (a[1] == "fan_in") r.fanIntake = v;
    else if (a[1] == "fan_up") r.fanStack = v;
    else return say("manual heater|fan_in|fan_up on|off");

    app.cmdManual(r.heater, r.fanIntake, r.fanStack);
    Serial.printf("manual heater=%s fan_in=%s fan_up=%s%s\n", yn(r.heater), yn(r.fanIntake),
                  yn(r.fanStack),
                  app.sm.mode() == Mode::Manual ? "" : "   (mode is not manual — 'mode manual')");
}

void Tui::handleLog(const std::vector<std::string>& a) {
    if (!app.logger) return say("logger not ready");
    if (a.size() < 2) return say("log now|list|dump <file> [n]|seed <hours>|rm <file>");

    const std::string& k = a[1];

    if (k == "now") {
        const Snapshot s = app.state.get();
        Serial.println(app.logger->writeRow(app.cfg, s) ? "row written" : "write failed");
        return;
    }

    if (k == "list") {
        const auto files = app.logger->files();
        if (files.empty()) return say("(no log files)");
        for (const auto& f : files) {
            Serial.printf("  %-16s %8u B\n", f.name.c_str(), static_cast<unsigned>(f.size));
        }
        if (app.fs) {
            Serial.printf("  fs: %u / %u B used\n", static_cast<unsigned>(app.fs->usedBytes()),
                          static_cast<unsigned>(app.fs->totalBytes()));
        }
        return;
    }

    if (k == "dump") {
        if (a.size() < 3) return say("log dump <file> [n]");
        const size_t n = (a.size() > 3) ? static_cast<size_t>(atoi(a[3].c_str())) : 20;
        if (app.fs) app.fs->flush();

        File f = LittleFS.open(("/logs/" + a[2]).c_str());
        if (!f) return sayf("no such file: %s\n", a[2].c_str());

        std::deque<String> tail;
        while (f.available()) {
            String line = f.readStringUntil('\n');
            tail.push_back(line);
            if (tail.size() > n) tail.pop_front();
        }
        f.close();
        for (const auto& l : tail) Serial.println(l);
        return;
    }

    if (k == "seed") {
        if (a.size() < 3) return say("log seed <hours>");
        const Snapshot s = app.state.get();
        if (!s.timeKnown) {
            return say("clock not set — connect the web page once, or POST /api/time");
        }
        const uint32_t hours = static_cast<uint32_t>(atoi(a[2].c_str()));
        Serial.printf("seeding %u h at 60 s spacing...\n", hours);
        const int n = app.logger->seed(app.cfg, hours, s.epoch);
        Serial.printf("%d rows written\n", n);
        return;
    }

    if (k == "rm") {
        if (a.size() < 3) return say("log rm <file>");
        Serial.println(app.logger->removeFile(a[2]) ? "deleted" : "failed");
        return;
    }

    Serial.println("log now|list|dump <file> [n]|seed <hours>|rm <file>");
}

void Tui::printHelp() {
    Serial.println(
        "help                       this list\n"
        "show                       one-shot state dump\n"
        "mon on|off                 stream one status line per second\n"
        "set setpoint|hyst|rh_target|loginterval|minon|minoff <v>\n"
        "save                       persist config to NVS\n"
        "mode off|auto|manual\n"
        "start [label]              begin a session\n"
        "stop                       stop -> cooldown\n"
        "reset                      clear latched fault\n"
        "manual heater|fan_in|fan_up on|off\n"
        "sim amb|ambrh|solar|moisture|speed|tcab <v>\n"
        "sim t_bot|rh_bot|t_top|rh_top <v>   override a sensor reading\n"
        "sim clear                  drop overrides\n"
        "fault sensor bot|top|both|none\n"
        "fault klixon on|off\n"
        "log now|list|dump <file> [n]|seed <hours>|rm <file>\n"
        "wifi                       ssid, ip, connected clients");
}

void Tui::printShow() {
    const Snapshot s = app.state.get();
    const Config& c = app.cfg;

    Serial.println("---------------------------------------------");
    Serial.printf("state     %-10s mode %-7s fault %s%s\n", stateName(s.state), modeName(s.mode),
                  faultName(s.fault), s.faultLatched ? " [LATCHED]" : "");
    if (s.botValid) {
        Serial.printf("bottom    %6.1f C  %5.1f %%RH  AH %5.2f g/m3\n", static_cast<double>(s.tBot),
                      static_cast<double>(s.rhBot), static_cast<double>(s.ahBot));
    } else {
        Serial.println("bottom    -- invalid --");
    }
    if (s.topValid) {
        Serial.printf("top       %6.1f C  %5.1f %%RH  AH %5.2f g/m3\n", static_cast<double>(s.tTop),
                      static_cast<double>(s.rhTop), static_cast<double>(s.ahTop));
    } else {
        Serial.println("top       -- invalid --");
    }
    if (s.botValid && s.topValid) {
        Serial.printf("removal   dAH %+5.2f g/m3\n", static_cast<double>(s.ahTop - s.ahBot));
    }
    Serial.printf("outputs   heater %-3s fan_in %-3s fan_up %-3s\n", yn(s.out.heater),
                  yn(s.out.fanIntake), yn(s.out.fanStack));
    Serial.printf("control   setpoint %.1f C  band %.1f-%.1f  rh_target %.0f %%\n",
                  static_cast<double>(c.setpointC),
                  static_cast<double>(c.setpointC - c.hysteresisC / 2.0f),
                  static_cast<double>(c.setpointC + c.hysteresisC / 2.0f),
                  static_cast<double>(c.rhTargetPct));
    Serial.printf("session   #%u %-12s elapsed %us %s\n", s.session.id,
                  s.session.label.empty() ? "(no label)" : s.session.label.c_str(), s.elapsedS,
                  s.session.active ? "active" : "");
    Serial.printf("clock     %s epoch=%u uptime=%us\n", s.timeKnown ? "set" : "UNSET", s.epoch,
                  s.uptimeS);
    Serial.printf("log       %s  %u rows this boot\n",
                  app.logger ? app.logger->currentPath().c_str() : "-",
                  app.logger ? app.logger->rowsWritten() : 0);
    if (app.plant) {
        Serial.printf("sim       moisture %.2f  amb %.1f C  solar %.2f  time x%.0f  ovr %s\n",
                      static_cast<double>(app.plant->moisture()),
                      static_cast<double>(app.plant->ambient()),
                      static_cast<double>(app.plant->solar()),
                      static_cast<double>(app.plant->speed()),
                      app.simSensors ? app.simSensors->overrideSummary().c_str() : "-");
    }
    Serial.printf("heap      %u B free\n", static_cast<unsigned>(ESP.getFreeHeap()));
    Serial.println("---------------------------------------------");
}

void Tui::printMonLine() {
    const Snapshot s = app.state.get();
    Serial.printf("%6us  %-8s  bot %5.1fC %5.1f%%  top %5.1fC %5.1f%%  H%d I%d U%d  m%.2f\n",
                  s.uptimeS, stateName(s.state), static_cast<double>(s.tBot),
                  static_cast<double>(s.rhBot), static_cast<double>(s.tTop),
                  static_cast<double>(s.rhTop), s.out.heater ? 1 : 0, s.out.fanIntake ? 1 : 0,
                  s.out.fanStack ? 1 : 0, static_cast<double>(s.moisture));
}

}  // namespace dh
