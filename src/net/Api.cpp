#include "net/Api.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "App.h"

namespace dh {

namespace {

String param(AsyncWebServerRequest* req, const char* name) {
    if (req->hasParam(name, true)) return req->getParam(name, true)->value();
    if (req->hasParam(name)) return req->getParam(name)->value();
    return String();
}

bool hasParam(AsyncWebServerRequest* req, const char* name) {
    return req->hasParam(name, true) || req->hasParam(name);
}

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

bool truthy(const String& v) { return v == "1" || v == "true" || v == "on" || v == "yes"; }

void sendOk(AsyncWebServerRequest* req) { req->send(200, "application/json", "{\"ok\":true}"); }

void sendErr(AsyncWebServerRequest* req, int code, const char* msg) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = msg;
    String out;
    serializeJson(doc, out);
    req->send(code, "application/json", out);
}

void configToJson(const Config& c, JsonObject o) {
    o["setpoint"] = c.setpointC;
    o["hyst"] = c.hysteresisC;
    o["rh_target"] = c.rhTargetPct;
    o["log_interval"] = c.logIntervalS;
    o["min_on"] = c.minOnS;
    o["min_off"] = c.minOffS;
    o["off_bot_t"] = c.offBotT;
    o["off_bot_rh"] = c.offBotRh;
    o["off_top_t"] = c.offTopT;
    o["off_top_rh"] = c.offTopRh;
    o["tz"] = c.tzOffsetMin;
    o["ap_ssid"] = c.apSsid;
}

}  // namespace

void Api::begin() {
    // ---- status -----------------------------------------------------------------
    server_.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        const Snapshot s = app.state.get();
        JsonDocument doc;

        // Fixed decimals, and a real JSON null when the sensor is out — the page must be
        // able to show "--" rather than a plausible-looking zero.
        auto num = [](bool valid, float v, int decimals) {
            return serialized(valid ? String(v, decimals) : String("null"));
        };
        doc["t_bot"] = num(s.botValid, s.tBot, 1);
        doc["rh_bot"] = num(s.botValid, s.rhBot, 1);
        doc["t_top"] = num(s.topValid, s.tTop, 1);
        doc["rh_top"] = num(s.topValid, s.rhTop, 1);
        doc["ah_bot"] = num(s.botValid, s.ahBot, 2);
        doc["ah_top"] = num(s.topValid, s.ahTop, 2);
        doc["vpd_top"] = num(s.topValid, s.vpdTop, 2);
        doc["bot_valid"] = s.botValid;
        doc["top_valid"] = s.topValid;

        doc["heater"] = s.out.heater;
        doc["fan_in"] = s.out.fanIntake;
        doc["fan_up"] = s.out.fanStack;

        doc["state"] = stateName(s.state);
        doc["mode"] = modeName(s.mode);
        doc["fault"] = faultName(s.fault);
        doc["fault_latched"] = s.faultLatched;

        JsonObject ses = doc["session"].to<JsonObject>();
        ses["id"] = s.session.id;
        ses["label"] = s.session.label.c_str();   // s outlives the serialize call
        ses["active"] = s.session.active;
        ses["elapsed_s"] = s.elapsedS;

        doc["setpoint"] = s.setpointC;
        doc["uptime_s"] = s.uptimeS;
        doc["epoch"] = s.epoch;
        doc["time_known"] = s.timeKnown;
        doc["moisture"] = s.moisture;
        doc["sim_speed"] = s.simSpeed;
        doc["heap"] = ESP.getFreeHeap();
        if (app.fs) {
            doc["fs_used"] = static_cast<uint32_t>(app.fs->usedBytes());
            doc["fs_total"] = static_cast<uint32_t>(app.fs->totalBytes());
        }

        auto* res = req->beginResponseStream("application/json");
        serializeJson(doc, *res);
        req->send(res);
    });

    // ---- config -----------------------------------------------------------------
    server_.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        configToJson(app.cfg, doc.to<JsonObject>());
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    server_.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* req) {
        Config& c = app.cfg;
        if (hasParam(req, "setpoint")) c.setpointC = clampf(param(req, "setpoint").toFloat(), 20.0f, 60.0f);
        if (hasParam(req, "hyst")) c.hysteresisC = clampf(param(req, "hyst").toFloat(), 0.2f, 10.0f);
        if (hasParam(req, "rh_target")) c.rhTargetPct = clampf(param(req, "rh_target").toFloat(), 5.0f, 90.0f);
        if (hasParam(req, "log_interval")) {
            c.logIntervalS = static_cast<uint32_t>(clampf(param(req, "log_interval").toFloat(), 1.0f, 3600.0f));
        }
        if (hasParam(req, "min_on")) c.minOnS = static_cast<uint32_t>(clampf(param(req, "min_on").toFloat(), 0.0f, 600.0f));
        if (hasParam(req, "min_off")) c.minOffS = static_cast<uint32_t>(clampf(param(req, "min_off").toFloat(), 0.0f, 600.0f));
        if (hasParam(req, "off_bot_t")) c.offBotT = clampf(param(req, "off_bot_t").toFloat(), -10.0f, 10.0f);
        if (hasParam(req, "off_bot_rh")) c.offBotRh = clampf(param(req, "off_bot_rh").toFloat(), -20.0f, 20.0f);
        if (hasParam(req, "off_top_t")) c.offTopT = clampf(param(req, "off_top_t").toFloat(), -10.0f, 10.0f);
        if (hasParam(req, "off_top_rh")) c.offTopRh = clampf(param(req, "off_top_rh").toFloat(), -20.0f, 20.0f);
        if (hasParam(req, "tz")) c.tzOffsetMin = static_cast<int32_t>(param(req, "tz").toInt());
        if (hasParam(req, "ap_ssid")) snprintf(c.apSsid, sizeof(c.apSsid), "%s", param(req, "ap_ssid").c_str());
        if (hasParam(req, "ap_pass")) {
            const String p = param(req, "ap_pass");
            if (p.length() == 0 || p.length() >= 8) {
                snprintf(c.apPass, sizeof(c.apPass), "%s", p.c_str());
            }
        }
        app.cmdSaveConfig();
        sendOk(req);
    });

    // ---- mode / manual ----------------------------------------------------------
    server_.on("/api/mode", HTTP_POST, [](AsyncWebServerRequest* req) {
        Mode m;
        if (!parseMode(std::string(param(req, "mode").c_str()), m)) {
            return sendErr(req, 400, "mode must be off, auto or manual");
        }
        if (!app.cmdMode(m)) return sendErr(req, 409, "fault latched — reset first");
        sendOk(req);
    });

    server_.on("/api/manual", HTTP_POST, [](AsyncWebServerRequest* req) {
        app.cmdManual(truthy(param(req, "heater")), truthy(param(req, "fan_in")),
                      truthy(param(req, "fan_up")));
        sendOk(req);
    });

    // ---- session ----------------------------------------------------------------
    server_.on("/api/session", HTTP_POST, [](AsyncWebServerRequest* req) {
        const String action = param(req, "action");
        if (action == "start") {
            if (!app.cmdStart(std::string(param(req, "label").c_str()))) {
                return sendErr(req, 409, "fault latched — reset first");
            }
            return sendOk(req);
        }
        if (action == "stop") {
            app.cmdStop();
            return sendOk(req);
        }
        sendErr(req, 400, "action must be start or stop");
    });

    server_.on("/api/fault/reset", HTTP_POST, [](AsyncWebServerRequest* req) {
        app.cmdResetFault();
        sendOk(req);
    });

    server_.on("/api/time", HTTP_POST, [](AsyncWebServerRequest* req) {
        const uint32_t epoch = static_cast<uint32_t>(param(req, "epoch").toInt());
        if (epoch < 1700000000UL) return sendErr(req, 400, "epoch looks wrong");
        app.cmdSetTime(epoch);
        sendOk(req);
    });

    // ---- logs -------------------------------------------------------------------
    server_.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        JsonArray arr = doc["files"].to<JsonArray>();
        if (app.logger) {
            for (const auto& f : app.logger->files()) {
                JsonObject o = arr.add<JsonObject>();
                o["name"] = f.name;
                o["size"] = static_cast<uint32_t>(f.size);
            }
        }
        if (app.fs) {
            doc["fs_used"] = static_cast<uint32_t>(app.fs->usedBytes());
            doc["fs_total"] = static_cast<uint32_t>(app.fs->totalBytes());
        }
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    server_.on("/api/logs/delete", HTTP_POST, [](AsyncWebServerRequest* req) {
        const String name = param(req, "file");
        if (name.length() == 0 || name.indexOf('/') >= 0) {
            return sendErr(req, 400, "bad file name");
        }
        if (!app.logger || !app.logger->removeFile(std::string(name.c_str()))) {
            return sendErr(req, 404, "not found");
        }
        sendOk(req);
    });

    // Chunked download straight off the filesystem. The current day's file may be missing
    // the last row or two if a write is in flight — harmless for export.
    server_.serveStatic("/logs/", LittleFS, "/logs/");

    // ---- page + captive portal ---------------------------------------------------
    server_.serveStatic("/", LittleFS, "/web/").setDefaultFile("index.html");

    server_.onNotFound([](AsyncWebServerRequest* req) {
        if (req->method() == HTTP_GET && !req->url().startsWith("/api/")) {
            req->redirect("http://192.168.4.1/");   // captive-portal nudge
            return;
        }
        sendErr(req, 404, "not found");
    });

    server_.begin();
    Serial.println("[web] server up on :80");
}

}  // namespace dh
