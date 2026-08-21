// CSV logging. ARCHITECTURE.md section 8.
//
// One file per local day, /logs/YYYYMMDD.csv, header written on creation. Rows are flushed
// as they are written; the store closes and reopens its handle every 32 rows so a power cut
// costs at most a few seconds of data.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/Types.h"
#include "hal/IFileStore.h"

namespace dh {

class Logger {
  public:
    explicit Logger(IFileStore& fs) : fs_(fs) {}

    bool begin(uint32_t nowMs);

    // Call every control tick. Writes a row when logIntervalS has elapsed.
    bool tick(const Config& cfg, const Snapshot& s, uint32_t nowMs);

    // Write one row right now (TUI "log now", session start/stop markers).
    bool writeRow(const Config& cfg, const Snapshot& s);
    bool writeComment(const Config& cfg, const Snapshot& s, const std::string& text);

    // Fabricate `hours` of plausible history ending at epochNow, for exercising the CSV
    // export path without waiting for a real run. Returns rows written.
    int seed(const Config& cfg, uint32_t hours, uint32_t epochNow);

    std::vector<FileEntry> files();
    bool removeFile(const std::string& name);
    std::string currentPath() const { return path_; }
    uint32_t rowsWritten() const { return rows_; }

    static const char* header();
    static std::string formatRow(const Snapshot& s, int64_t ts);
    static std::string pathForEpoch(uint32_t epoch, int32_t tzOffsetMin);
    static std::string dayStamp(uint32_t epoch, int32_t tzOffsetMin);

  private:
    bool ensureFile(const std::string& path);
    void rotateIfNeeded();

    IFileStore& fs_;
    std::string path_;
    uint32_t lastWriteMs_ = 0;
    bool timeWasKnown_ = false;
    uint32_t rows_ = 0;
};

}  // namespace dh
