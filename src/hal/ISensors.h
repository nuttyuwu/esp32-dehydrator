#pragma once

#include "core/Types.h"

namespace dh {

class ISensors {
  public:
    virtual ~ISensors() = default;
    virtual bool begin() = 0;
    virtual Reading readBottom() = 0;
    virtual Reading readTop() = 0;
};

}  // namespace dh
