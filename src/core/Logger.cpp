#include "core/Logger.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/Metrics.h"

namespace dh {

namespace {

constexpr const char* kDir = "/logs";
constexpr float kFreeFraction = 0.15f;   // rotate below 15 % free

// Howard Hinnant's civil_from_days. No libc time functions: gmtime_r is not portable to
// every host toolchain and this has to compile natively for the unit tests.
void civilFromDays(int64_t z, int& y, unsigned& m, unsigned& d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const uint64_t doe = static_cast<uint64_t>(z - era * 146097);
    const uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t yy = static_cast<int64_t>(yoe) + era * 400;
    const uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const uint64_t mp = (5 * doy + 2) / 153;
    d = static_cast<unsigned>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<unsigned>(mp + (mp < 10 ? 3 : -9));
    y = static_cast<int>(yy + (m <= 2));
}

// "43.8" for a valid reading, "" for an invalid one — a blank cell in Excel is honest,
// a zero is a lie.
void appendFloat(std::string& out, float v, bool valid, int decimals) {
    char buf[24];
    if (!valid) return;
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, static_cast<double>(v));
    out += buf;
}

}  // namespace

const char* Logger::header() {
    return "ts,t_bot,rh_bot,t_top,rh_top,ah_bot,ah_top,heater,fan_in,fan_up,state,session";
}

std::string Logger::dayStamp(uint32_t epoch, int32_t tzOffsetMin) {
    const int64_t local = static_cast<int64_t>(epoch) + static_cast<int64_t>(tzOffsetMin) * 60;
    int y = 1970;
    unsigned m = 1, d = 1;
    civilFromDays(local / 86400, y, m, d);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d%02u%02u", y, m, d);
    return std::string(buf);
}

std::string Logger::pathForEpoch(uint32_t epoch, int32_t tzOffsetMin) {
    if (epoch == 0) return std::string(kDir) + "/nodate.csv";
    return std::string(kDir) + "/" + dayStamp(epoch, tzOffsetMin) + ".csv";
}

std::string Logger::formatRow(const Snapshot& s, int64_t ts) {
    std::string row;
    row.reserve(96);

    char tsbuf[24];
    std::snprintf(tsbuf, sizeof(tsbuf), "%lld", static_cast<long long>(ts));
    row += tsbuf;

    row += ',';
    appendFloat(row, s.tBot, s.botValid, 1);
    row += ',';
    appendFloat(row, s.rhBot, s.botValid, 1);
    row += ',';
    appendFloat(row, s.tTop, s.topValid, 1);
    row += ',';
    appendFloat(row, s.rhTop, s.topValid, 1);
    row += ',';
    appendFloat(row, s.ahBot, s.botValid, 2);
    row += ',';
    appendFloat(row, s.ahTop, s.topValid, 2);

    row += s.out.heater ? ",1" : ",0";
    row += s.out.fanIntake ? ",1" : ",0";
    row += s.out.fanStack ? ",1" : ",0";

    row += ',';
    row += stateName(s.state);

    char idbuf[16];
    std::snprintf(idbuf, sizeof(idbuf), ",%u", static_cast<unsigned>(s.session.id));
    row += idbuf;

    return row;
}

bool Logger::begin(uint32_t nowMs) {
    lastWriteMs_ = nowMs;
    rows_ = 0;
    path_.clear();
    timeWasKnown_ = false;
    return true;
}

bool Logger::ensureFile(const std::string& path) {
    if (fs_.exists(path)) return true;
    return fs_.appendLine(path, header());
}

void Logger::rotateIfNeeded() {
    const size_t total = fs_.totalBytes();
    if (total == 0) return;
    if (static_cast<float>(fs_.freeBytes()) >= kFreeFraction * static_cast<float>(total)) return;

    auto entries = fs_.list(kDir);
    if (entries.size() <= 1) return;   // never delete the file we are writing into

    // Names are YYYYMMDD.csv so lexicographic order is chronological; nodate.csv is the
    // pre-clock file and goes first.
    std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
        const bool an = a.name.rfind("nodate", 0) == 0;
        const bool bn = b.name.rfind("nodate", 0) == 0;
        if (an != bn) return an;
        return a.name < b.name;
    });

    for (const auto& e : entries) {
        const std::string full = std::string(kDir) + "/" + e.name;
        if (full == path_) continue;
        fs_.remove(full);
        if (static_cast<float>(fs_.freeBytes()) >= kFreeFraction * static_cast<float>(total)) {
            break;
        }
    }
}

bool Logger::writeRow(const Config& cfg, const Snapshot& s) {
    const std::string want = pathForEpoch(s.timeKnown ? s.epoch : 0, cfg.tzOffsetMin);
    const bool newFile = (want != path_);

    if (newFile) {
        rotateIfNeeded();
        path_ = want;
        if (!ensureFile(path_)) return false;
        if (s.timeKnown && !timeWasKnown_ && rows_ > 0) {
            fs_.appendLine(path_, "# time set");
        }
    }
    if (s.timeKnown) timeWasKnown_ = true;

    const int64_t ts = s.timeKnown ? static_cast<int64_t>(s.epoch)
                                   : -static_cast<int64_t>(s.uptimeS);
    if (!fs_.appendLine(path_, formatRow(s, ts))) return false;
    rows_++;
    return true;
}

bool Logger::writeComment(const Config& cfg, const Snapshot& s, const std::string& text) {
    const std::string want = pathForEpoch(s.timeKnown ? s.epoch : 0, cfg.tzOffsetMin);
    if (want != path_) {
        path_ = want;
        if (!ensureFile(path_)) return false;
    }
    return fs_.appendLine(path_, "# " + text);
}

bool Logger::tick(const Config& cfg, const Snapshot& s, uint32_t nowMs) {
    const uint32_t intervalMs = (cfg.logIntervalS ? cfg.logIntervalS : 10u) * 1000u;
    if ((nowMs - lastWriteMs_) < intervalMs) return false;
    lastWriteMs_ = nowMs;
    return writeRow(cfg, s);
}

int Logger::seed(const Config& cfg, uint32_t hours, uint32_t epochNow) {
    if (hours == 0 || epochNow == 0) return 0;

    // 60 s spacing regardless of logIntervalS — 24 h at 10 s is 8640 rows and LittleFS is
    // slow enough that seeding would take minutes.
    constexpr uint32_t kStepS = 60;
    const uint32_t rows = hours * (3600 / kStepS);
    const uint32_t startEpoch = epochNow - hours * 3600;

    Snapshot s;
    s.timeKnown = true;
    s.session.id = 900;   // 9xx marks fabricated data
    s.botValid = true;
    s.topValid = true;

    int written = 0;
    for (uint32_t i = 0; i < rows; i++) {
        const uint32_t epoch = startEpoch + i * kStepS;
        const float hoursIn = static_cast<float>(i) * kStepS / 3600.0f;
        const float moisture = std::exp(-hoursIn / 3.0f);
        const bool heating = hoursIn > 0.15f;

        s.epoch = epoch;
        s.tBot = heating ? cfg.setpointC + ((i % 7) - 3) * 0.15f : 22.0f + hoursIn * 20.0f;
        s.rhBot = 12.0f + 6.0f * moisture;
        s.tTop = s.tBot - 2.0f - 3.0f * moisture;
        s.rhTop = 10.0f + 70.0f * moisture;
        s.ahBot = metrics::absHumidity(s.tBot, s.rhBot);
        s.ahTop = metrics::absHumidity(s.tTop, s.rhTop);
        s.out.heater = heating && (i % 4 != 0);
        s.out.fanIntake = heating;
        s.out.fanStack = true;
        s.state = heating ? (moisture < 0.2f ? State::Drying : State::Preheat) : State::Preheat;

        const std::string want = pathForEpoch(epoch, cfg.tzOffsetMin);
        if (want != path_) {
            path_ = want;
            if (!ensureFile(path_)) return written;
        }
        if (!fs_.appendLine(path_, formatRow(s, static_cast<int64_t>(epoch)))) return written;
        written++;
    }
    fs_.flush();
    path_.clear();   // force the live logger to reopen its own file
    return written;
}

std::vector<FileEntry> Logger::files() { return fs_.list(kDir); }

bool Logger::removeFile(const std::string& name) {
    const std::string full = std::string(kDir) + "/" + name;
    if (full == path_) {
        fs_.flush();
        path_.clear();
    }
    return fs_.remove(full);
}

}  // namespace dh
