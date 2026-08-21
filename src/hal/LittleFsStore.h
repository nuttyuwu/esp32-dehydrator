#pragma once

#include <FS.h>
#include <LittleFS.h>

#include "hal/IFileStore.h"

namespace dh {

// Keeps one file handle open across appends and closes it every kRowsPerClose rows, so a
// power cut costs a few seconds of data instead of the whole file's directory entry.
class LittleFsStore : public IFileStore {
  public:
    bool begin() override;
    bool appendLine(const std::string& path, const std::string& line) override;
    void flush() override;
    bool exists(const std::string& path) override;
    bool remove(const std::string& path) override;
    std::vector<FileEntry> list(const std::string& dir) override;
    size_t totalBytes() override;
    size_t usedBytes() override;

  private:
    static constexpr int kRowsPerClose = 32;

    void closeHandle();

    File f_;
    std::string openPath_;
    int sinceClose_ = 0;
    bool ready_ = false;
};

}  // namespace dh
