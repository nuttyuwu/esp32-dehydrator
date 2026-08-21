#pragma once

#include <ESPAsyncWebServer.h>

namespace dh {

// Every route from ARCHITECTURE.md section 10, plus the single page served from LittleFS
// /web. POST bodies are form-urlencoded rather than JSON — the page has no framework and
// URLSearchParams is one line of JS.
//
// Deviation from the doc, noted in README.md: CSV files download from /logs/<name> via
// serveStatic instead of /api/logs/<name>, which keeps the build free of the
// ASYNCWEBSERVER_REGEX flag and gets true chunked streaming for free.
class Api {
  public:
    void begin();

  private:
    AsyncWebServer server_{80};
};

}  // namespace dh
