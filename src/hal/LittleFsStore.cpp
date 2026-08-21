#include "hal/LittleFsStore.h"

#include <Arduino.h>

namespace dh {

bool LittleFsStore::begin() {
    if (!LittleFS.begin(true)) {   // true = format if the partition is blank
        Serial.println("[fs] LittleFS mount failed");
        ready_ = false;
        return false;
    }
    if (!LittleFS.exists("/logs")) LittleFS.mkdir("/logs");
    ready_ = true;
    Serial.printf("[fs] LittleFS %u / %u bytes used\n", static_cast<unsigned>(usedBytes()),
                  static_cast<unsigned>(totalBytes()));
    return true;
}

void LittleFsStore::closeHandle() {
    if (f_) f_.close();
    openPath_.clear();
    sinceClose_ = 0;
}

bool LittleFsStore::appendLine(const std::string& path, const std::string& line) {
    if (!ready_) return false;

    if (openPath_ != path) {
        closeHandle();
        f_ = LittleFS.open(path.c_str(), FILE_APPEND);
        if (!f_) {
            Serial.printf("[fs] open failed: %s\n", path.c_str());
            return false;
        }
        openPath_ = path;
    }

    const size_t n = f_.print(line.c_str());
    f_.print('\n');
    f_.flush();

    if (++sinceClose_ >= kRowsPerClose) closeHandle();
    return n == line.size();
}

void LittleFsStore::flush() {
    if (f_) f_.flush();
    closeHandle();
}

bool LittleFsStore::exists(const std::string& path) {
    if (!ready_) return false;
    if (openPath_ == path) return true;
    return LittleFS.exists(path.c_str());
}

bool LittleFsStore::remove(const std::string& path) {
    if (!ready_) return false;
    if (openPath_ == path) closeHandle();
    return LittleFS.remove(path.c_str());
}

std::vector<FileEntry> LittleFsStore::list(const std::string& dir) {
    std::vector<FileEntry> out;
    if (!ready_) return out;

    // A file we are holding open reports a stale size; flush so the listing is truthful.
    if (!openPath_.empty()) flush();

    File d = LittleFS.open(dir.c_str());
    if (!d || !d.isDirectory()) return out;

    File f = d.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            std::string name = f.name();
            const size_t slash = name.find_last_of('/');
            if (slash != std::string::npos) name = name.substr(slash + 1);
            out.push_back(FileEntry{name, static_cast<size_t>(f.size())});
        }
        f = d.openNextFile();
    }
    d.close();
    return out;
}

size_t LittleFsStore::totalBytes() { return ready_ ? LittleFS.totalBytes() : 0; }
size_t LittleFsStore::usedBytes() { return ready_ ? LittleFS.usedBytes() : 0; }

}  // namespace dh
