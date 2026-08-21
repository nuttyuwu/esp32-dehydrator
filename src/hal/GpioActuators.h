#pragma once

#include "hal/IActuators.h"

namespace dh {

// Real relays / SSR. Compiled into env:hw only. Unvalidated until the hardware exists —
// check relay idle polarity with a multimeter before the element is on mains.
class GpioActuators : public ActuatorBase {
  public:
    bool begin() override;
    bool klixonOpen() override;

  protected:
    void write(const Outputs& o) override;

  private:
    static void driveRelay(int pin, bool on);
};

}  // namespace dh
