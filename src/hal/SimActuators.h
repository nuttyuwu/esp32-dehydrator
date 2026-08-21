#pragma once

#include "hal/IActuators.h"
#include "sim/Plant.h"

namespace dh {

class SimActuators : public ActuatorBase {
  public:
    explicit SimActuators(sim::Plant& plant) : plant_(plant) {}

    bool begin() override { return true; }
    bool klixonOpen() override { return klixon_; }
    void setKlixon(bool open) { klixon_ = open; }

  protected:
    void write(const Outputs& o) override { plant_.setOutputs(o); }
    void onChange(const Outputs& o) override;

  private:
    sim::Plant& plant_;
    bool klixon_ = false;
};

}  // namespace dh
