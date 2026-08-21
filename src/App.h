// Everything the tasks share, plus the command entry points. The web API and the serial
// TUI both go through these — one implementation of "start a session", not two.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <string>

#include "ConfigStore.h"
#include "StateStore.h"
#include "core/Logger.h"
#include "core/SensorHub.h"
#include "core/StateMachine.h"
#include "core/Types.h"
#include "hal/Clock.h"
#include "hal/IActuators.h"
#include "hal/IFileStore.h"
#include "hal/ISensors.h"

#if BUILD_SIM
#include "hal/SimActuators.h"
#include "hal/SimSensors.h"
#include "sim/Plant.h"
#endif

namespace dh {

struct App {
    Config cfg;
    ConfigStore store;
    StateStore state;
    Clock clock;
    SensorHub hub;
    StateMachine sm;

    IFileStore* fs = nullptr;
    Logger* logger = nullptr;
    ISensors* sensors = nullptr;
    IActuators* actuators = nullptr;

#if BUILD_SIM
    sim::Plant* plant = nullptr;
    SimSensors* simSensors = nullptr;
    SimActuators* simActuators = nullptr;
#endif

    SemaphoreHandle_t cmdMtx = nullptr;
    bool bootFaultWasWatchdog = false;

    void begin();

    // Periodic work, each called from its own task.
    void sensorTick();
    void controlTick();
    void loggerTick();

    // Commands — safe to call from any task.
    bool cmdStart(const std::string& label);
    void cmdStop();
    bool cmdMode(Mode m);
    void cmdManual(bool heater, bool fanIntake, bool fanStack);
    void cmdResetFault();
    void cmdSaveConfig();
    void cmdSetTime(uint32_t epoch);

    // Snapshot builder used by the control task.
    Snapshot buildSnapshot() const;

  private:
    class Lock {
      public:
        explicit Lock(SemaphoreHandle_t m) : m_(m) {
            if (m_) xSemaphoreTake(m_, portMAX_DELAY);
        }
        ~Lock() {
            if (m_) xSemaphoreGive(m_);
        }

      private:
        SemaphoreHandle_t m_;
    };
};

extern App app;

}  // namespace dh
