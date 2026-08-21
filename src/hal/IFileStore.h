// Storage seam. core/Logger.cpp talks to this, never to LittleFS directly — that is what
// keeps the logger testable on the host and keeps ARCHITECTURE.md section 3's layering rule
// honest.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace dh {

struct FileEntry {
    std::string name;   // bare name, no directory
    size_t size = 0;
};

class IFileStore {
  public:
    virtual ~IFileStore() = default;
    virtual bool begin() = 0;

    // Appends one line plus '\n'. The implementation owns any handle caching.
    virtual bool appendLine(const std::string& path, const std::string& line) = 0;
    virtual void flush() = 0;

    virtual bool exists(const std::string& path) = 0;
    virtual bool remove(const std::string& path) = 0;
    virtual std::vector<FileEntry> list(const std::string& dir) = 0;

    virtual size_t totalBytes() = 0;
    virtual size_t usedBytes() = 0;
    size_t freeBytes() { return totalBytes() - usedBytes(); }
};

}  // namespace dh
