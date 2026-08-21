// The one mutex-guarded copy of system state. Readers (web handlers, TUI, logger) take a
// value copy and let go immediately; nobody reads the live struct.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "core/Types.h"

namespace dh {

class StateStore {
  public:
    void begin() { mtx_ = xSemaphoreCreateMutex(); }

    void set(const Snapshot& s) {
        if (!mtx_) return;
        if (xSemaphoreTake(mtx_, pdMS_TO_TICKS(50)) == pdTRUE) {
            snap_ = s;
            xSemaphoreGive(mtx_);
        }
    }

    Snapshot get() const {
        Snapshot copy;
        if (!mtx_) return copy;
        if (xSemaphoreTake(mtx_, pdMS_TO_TICKS(50)) == pdTRUE) {
            copy = snap_;
            xSemaphoreGive(mtx_);
        }
        return copy;
    }

  private:
    mutable SemaphoreHandle_t mtx_ = nullptr;
    Snapshot snap_;
};

}  // namespace dh
