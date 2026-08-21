#include "App.h"

#include <Arduino.h>

#include "core/Metrics.h"

namespace dh {

App app;

void App::begin() { cmdMtx = xSemaphoreCreateMutex(); }

void App::sensorTick() {
#if BUILD_SIM
    if (plant && actuators) {
        plant->setOutputs(actuators->current());
        plant->step(1.0f);
    }
#endif
    if (!sensors) return;
    const Reading b = sensors->readBottom();
    const Reading t = sensors->readTop();

    Lock lk(cmdMtx);
    hub.update(b, t, cfg, millis());
}

void App::controlTick() {
    Lock lk(cmdMtx);

    TickInputs in;
    in.nowMs = millis();
    in.klixonOpen = actuators ? actuators->klixonOpen() : false;
    in.collectorValid = false;   // no collector sensor in v1

    const bool wasActive = sm.session().active;
    const Outputs o = sm.tick(cfg, hub, in);
    if (actuators) actuators->apply(o);

    // The session flag is what a reboot reads to decide whether to resume.
    if (wasActive != sm.session().active) {
        if (sm.session().active) {
            store.saveSession(sm.session());
        } else {
            store.clearSession();
        }
    }

    state.set(buildSnapshot());
}

void App::loggerTick() {
    if (!logger) return;
    const Snapshot s = state.get();
    logger->tick(cfg, s, millis());
}

bool App::cmdStart(const std::string& label) {
    Lock lk(cmdMtx);
    const bool ok = sm.start(label, millis(), clock.epoch(), clock.uptimeS());
    if (ok) store.saveSession(sm.session());
    return ok;
}

void App::cmdStop() {
    Lock lk(cmdMtx);
    sm.stop(millis());
    store.clearSession();
}

bool App::cmdMode(Mode m) {
    Lock lk(cmdMtx);
    if (sm.faultLatched()) return false;
    sm.setMode(m, millis());
    if (m == Mode::Off) store.clearSession();
    return true;
}

void App::cmdManual(bool heater, bool fanIntake, bool fanStack) {
    Lock lk(cmdMtx);
    ManualRequest r;
    r.heater = heater;
    r.fanIntake = fanIntake;
    r.fanStack = fanStack;
    sm.setManual(r);
}

void App::cmdResetFault() {
    Lock lk(cmdMtx);
    sm.resetFault(millis());
    store.clearSession();
}

void App::cmdSaveConfig() {
    Lock lk(cmdMtx);
    store.save(cfg);
}

void App::cmdSetTime(uint32_t epoch) {
    Lock lk(cmdMtx);
    clock.setEpoch(epoch);
}

Snapshot App::buildSnapshot() const {
    Snapshot s;
    const Reading& b = hub.bottom();
    const Reading& t = hub.top();

    s.tBot = b.t;
    s.rhBot = b.rh;
    s.botValid = b.valid;
    s.tTop = t.t;
    s.rhTop = t.rh;
    s.topValid = t.valid;

    if (b.valid) s.ahBot = metrics::absHumidity(b.t, b.rh);
    if (t.valid) {
        s.ahTop = metrics::absHumidity(t.t, t.rh);
        s.vpdTop = metrics::vpdKPa(t.t, t.rh);
    }

    s.out = actuators ? actuators->current() : Outputs{};
    s.state = sm.state();
    s.mode = sm.mode();
    s.fault = sm.fault();
    s.faultLatched = sm.faultLatched();
    s.session = sm.session();
    s.uptimeS = clock.uptimeS();
    s.elapsedS = sm.elapsedS(s.uptimeS);
    s.epoch = clock.epoch();
    s.timeKnown = clock.timeKnown();
    s.setpointC = cfg.setpointC;

#if BUILD_SIM
    if (plant) {
        s.moisture = plant->moisture();
        s.simSpeed = plant->speed();
    }
#endif
    return s;
}

}  // namespace dh
