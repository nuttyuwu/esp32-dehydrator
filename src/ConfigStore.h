// Config and session persistence in NVS.
#pragma once

#include "core/Types.h"

namespace dh {

class ConfigStore {
  public:
    bool begin();

    void load(Config& cfg);
    void save(const Config& cfg);

    void loadSession(Session& s);
    void saveSession(const Session& s);
    void clearSession();

  private:
    bool ok_ = false;
};

}  // namespace dh
