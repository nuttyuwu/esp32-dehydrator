#pragma once

#include "core/Types.h"

namespace dh {

class IActuators {
  public:
    virtual ~IActuators() = default;
    virtual bool begin() = 0;
    virtual void apply(const Outputs& o) = 0;
    virtual Outputs current() const = 0;
    virtual bool klixonOpen() = 0;
};

// Both the real and the simulated actuators derive from this, so the heater/fan interlock
// exists in exactly one place. Subclasses implement write(), never apply().
class ActuatorBase : public IActuators {
  public:
    void apply(const Outputs& requested) final {
        Outputs safe = requested;
        if (safe.heater && !safe.fanStack) {
            safe.heater = false;   // no up-flow, no heat. Not negotiable.
        }
        if (safe != cur_) {
            cur_ = safe;
            onChange(safe);
        }
        write(safe);
    }

    Outputs current() const override { return cur_; }

  protected:
    virtual void write(const Outputs& o) = 0;
    virtual void onChange(const Outputs&) {}

    Outputs cur_;
};

}  // namespace dh
