// Hybrid solar/electric food dehydrator — ESP32-WROVER.
// Wiring, control law, and scope: see ARCHITECTURE.md.

#include <Arduino.h>
#include <esp_task_wdt.h>

#include "App.h"
#include "hal/LittleFsStore.h"
#include "net/Api.h"
#include "net/WifiAp.h"

#if BUILD_SIM
#include "hal/SimActuators.h"
#include "hal/SimSensors.h"
#include "sim/Plant.h"
#include "tui/Tui.h"
#else
#include "hal/GpioActuators.h"
#include "hal/Sht31Sensors.h"
#endif

namespace {

dh::LittleFsStore g_fs;
dh::Logger g_logger(g_fs);
dh::WifiAp g_wifi;
dh::Api g_api;

#if BUILD_SIM
dh::sim::Plant g_plant;
dh::SimSensors g_sensors(g_plant);
dh::SimActuators g_actuators(g_plant);
dh::Tui g_tui;
#else
dh::Sht31Sensors g_sensors;
dh::GpioActuators g_actuators;
#endif

constexpr uint32_t kWdtTimeoutS = 15;

bool cameBackFromWatchdog() {
    switch (esp_reset_reason()) {
        case ESP_RST_TASK_WDT:
        case ESP_RST_INT_WDT:
        case ESP_RST_WDT:
        case ESP_RST_PANIC:
            return true;
        default:
            return false;
    }
}

void sensorTask(void*) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        dh::app.sensorTick();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));
    }
}

void controlTask(void*) {
    esp_task_wdt_add(nullptr);
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        dh::app.controlTick();
        esp_task_wdt_reset();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));
    }
}

void loggerTask(void*) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        dh::app.loggerTick();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));
    }
}

void netTask(void*) {
    for (;;) {
        g_wifi.loop();
#if BUILD_SIM
        g_tui.poll();
#endif
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
#if BUILD_SIM
    Serial.println("=== dehydrator firmware — SIM build (no hardware required) ===");
#else
    Serial.println("=== dehydrator firmware — HW build ===");
#endif

    const bool wdtReset = cameBackFromWatchdog();
    if (wdtReset) Serial.println("[boot] recovered from watchdog/panic — starting latched");

    dh::app.begin();

    dh::app.store.begin();
    dh::app.store.load(dh::app.cfg);

    g_fs.begin();
    dh::app.fs = &g_fs;
    dh::app.logger = &g_logger;
    g_logger.begin(millis());

    dh::app.sensors = &g_sensors;
    dh::app.actuators = &g_actuators;
    g_sensors.begin();
    g_actuators.begin();
    g_actuators.apply(dh::Outputs{});   // everything off before anything else happens

#if BUILD_SIM
    dh::app.plant = &g_plant;
    dh::app.simSensors = &g_sensors;
    dh::app.simActuators = &g_actuators;
    g_plant.setAmbient(22.0f);
    g_plant.reset();
#endif

    dh::app.clock.begin();
    dh::app.state.begin();
    dh::app.hub.begin(millis());
    dh::app.sm.begin(millis(), wdtReset);

    dh::Session saved;
    dh::app.store.loadSession(saved);
    if (saved.id != 0) {
        saved.startedAtUptimeS = dh::app.clock.uptimeS();
        dh::app.sm.restoreSession(saved, millis());
        if (saved.active) {
            Serial.printf("[boot] resuming session #%u (%s)\n", saved.id,
                          saved.label.empty() ? "no label" : saved.label.c_str());
        }
    }

    // Publish one snapshot before anything can read it.
    dh::app.state.set(dh::app.buildSnapshot());

    g_wifi.begin(dh::app.cfg);
    g_api.begin();

#if BUILD_SIM
    g_tui.begin(&g_wifi);
#endif

    esp_task_wdt_init(kWdtTimeoutS, true);

    xTaskCreatePinnedToCore(sensorTask, "sensor", 4096, nullptr, 3, nullptr, 1);
    xTaskCreatePinnedToCore(controlTask, "control", 4096, nullptr, 4, nullptr, 1);
    xTaskCreatePinnedToCore(loggerTask, "logger", 6144, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(netTask, "net", 6144, nullptr, 1, nullptr, 0);

    Serial.println("[boot] ready");
#if BUILD_SIM
    Serial.println("type 'help' for the console");
#endif
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
