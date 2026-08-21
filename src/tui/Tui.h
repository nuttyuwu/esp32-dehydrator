// Line-based serial console for the sim build. ARCHITECTURE.md section 11.
// 115200 8N1, '\n'-terminated commands, no ANSI cursor control.
#pragma once

#include <string>
#include <vector>

#include "net/WifiAp.h"

namespace dh {

class Tui {
  public:
    void begin(WifiAp* wifi);
    void poll();   // call often; non-blocking

  private:
    void dispatch(const std::string& line);
    void handleSet(const std::vector<std::string>& a);
    void handleSim(const std::vector<std::string>& a);
    void handleFault(const std::vector<std::string>& a);
    void handleManual(const std::vector<std::string>& a);
    void handleLog(const std::vector<std::string>& a);
    void printHelp();
    void printShow();
    void printMonLine();

    WifiAp* wifi_ = nullptr;
    std::string buf_;
    bool mon_ = false;
    uint32_t lastMonMs_ = 0;
};

}  // namespace dh
