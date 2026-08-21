// CSV schema, file naming, and rotation, against an in-memory file store.
#include <unity.h>

#include <map>

#include "core/Logger.h"

using namespace dh;

namespace {

class FakeStore : public IFileStore {
  public:
    std::map<std::string, std::vector<std::string>> files;
    size_t capacity = 100000;

    bool begin() override { return true; }

    bool appendLine(const std::string& path, const std::string& line) override {
        files[path].push_back(line);
        return true;
    }
    void flush() override {}
    bool exists(const std::string& path) override { return files.count(path) > 0; }
    bool remove(const std::string& path) override { return files.erase(path) > 0; }

    std::vector<FileEntry> list(const std::string& dir) override {
        std::vector<FileEntry> out;
        for (const auto& kv : files) {
            if (kv.first.rfind(dir + "/", 0) != 0) continue;
            size_t bytes = 0;
            for (const auto& l : kv.second) bytes += l.size() + 1;
            out.push_back(FileEntry{kv.first.substr(dir.size() + 1), bytes});
        }
        return out;
    }

    size_t totalBytes() override { return capacity; }
    size_t usedBytes() override {
        size_t n = 0;
        for (const auto& kv : files) {
            for (const auto& l : kv.second) n += l.size() + 1;
        }
        return n;
    }

    size_t lineCount(const std::string& path) {
        return files.count(path) ? files[path].size() : 0;
    }
};

constexpr uint32_t kEpoch = 1755660000;   // 2025-08-20 03:20 UTC, 11:20 in UB (+8)

Snapshot liveSnapshot() {
    Snapshot s;
    s.tBot = 43.8f;
    s.rhBot = 61.2f;
    s.tTop = 41.1f;
    s.rhTop = 74.5f;
    s.ahBot = 26.9f;
    s.ahTop = 34.2f;
    s.botValid = true;
    s.topValid = true;
    s.out.heater = true;
    s.out.fanIntake = true;
    s.out.fanStack = true;
    s.state = State::Drying;
    s.session.id = 7;
    s.epoch = kEpoch;
    s.timeKnown = true;
    return s;
}

}  // namespace

void setUp() {}
void tearDown() {}

static void test_day_stamp_uses_the_local_timezone() {
    TEST_ASSERT_EQUAL_STRING("20250820", Logger::dayStamp(kEpoch, 480).c_str());
    // 03:20 UTC is still the previous day in UTC-8.
    TEST_ASSERT_EQUAL_STRING("20250819", Logger::dayStamp(kEpoch, -480).c_str());
    TEST_ASSERT_EQUAL_STRING("/logs/20250820.csv", Logger::pathForEpoch(kEpoch, 480).c_str());
    TEST_ASSERT_EQUAL_STRING("/logs/nodate.csv", Logger::pathForEpoch(0, 480).c_str());
}

static void test_row_matches_the_documented_schema() {
    const Snapshot s = liveSnapshot();
    const std::string row = Logger::formatRow(s, static_cast<int64_t>(kEpoch));
    TEST_ASSERT_EQUAL_STRING("1755660000,43.8,61.2,41.1,74.5,26.90,34.20,1,1,1,DRYING,7",
                             row.c_str());
}

static void test_invalid_sensors_leave_blank_cells_not_zeros() {
    Snapshot s = liveSnapshot();
    s.botValid = false;
    const std::string row = Logger::formatRow(s, 12345);
    TEST_ASSERT_EQUAL_STRING("12345,,,41.1,74.5,,34.20,1,1,1,DRYING,7", row.c_str());
}

static void test_header_written_once_then_rows_appended() {
    FakeStore fs;
    Logger log(fs);
    log.begin(0);

    const Snapshot s = liveSnapshot();
    Config cfg;
    for (int i = 0; i < 5; i++) TEST_ASSERT_TRUE(log.writeRow(cfg, s));

    const std::string path = "/logs/20250820.csv";
    TEST_ASSERT_EQUAL_UINT32(6, fs.lineCount(path));   // header + 5 rows
    TEST_ASSERT_EQUAL_STRING(Logger::header(), fs.files[path][0].c_str());
    TEST_ASSERT_EQUAL_UINT32(5, log.rowsWritten());
}

static void test_unknown_time_logs_negative_uptime_then_switches_file() {
    FakeStore fs;
    Logger log(fs);
    log.begin(0);
    Config cfg;

    Snapshot s = liveSnapshot();
    s.timeKnown = false;
    s.epoch = 0;
    s.uptimeS = 42;
    log.writeRow(cfg, s);

    TEST_ASSERT_EQUAL_UINT32(2, fs.lineCount("/logs/nodate.csv"));
    TEST_ASSERT_TRUE(fs.files["/logs/nodate.csv"][1].rfind("-42,", 0) == 0);

    // Browser sets the clock: a correctly named file starts, with the transition marked.
    s.timeKnown = true;
    s.epoch = kEpoch;
    log.writeRow(cfg, s);

    const auto& lines = fs.files["/logs/20250820.csv"];
    TEST_ASSERT_EQUAL_UINT32(3, lines.size());
    TEST_ASSERT_EQUAL_STRING("# time set", lines[1].c_str());
}

static void test_tick_respects_the_log_interval() {
    FakeStore fs;
    Logger log(fs);
    Config cfg;
    cfg.logIntervalS = 10;
    const Snapshot s = liveSnapshot();

    uint32_t t = 0;
    log.begin(t);
    for (int i = 0; i < 60; i++) {
        t += 1000;
        log.tick(cfg, s, t);
    }
    TEST_ASSERT_EQUAL_UINT32(6, log.rowsWritten());
}

static void test_rotation_deletes_the_oldest_file_first() {
    FakeStore fs;
    fs.capacity = 4000;
    Logger log(fs);
    Config cfg;
    log.begin(0);

    // Three days of history already on the card, nearly full.
    for (const char* day : {"20250801", "20250802", "20250803"}) {
        const std::string p = std::string("/logs/") + day + ".csv";
        fs.files[p].push_back(Logger::header());
        for (int i = 0; i < 20; i++) fs.files[p].push_back(std::string(60, 'x'));
    }
    TEST_ASSERT_TRUE(fs.usedBytes() > 3400);

    Snapshot s = liveSnapshot();
    log.writeRow(cfg, s);

    TEST_ASSERT_FALSE(fs.exists("/logs/20250801.csv"));   // oldest gone
    TEST_ASSERT_TRUE(fs.exists("/logs/20250803.csv"));    // newest kept
    TEST_ASSERT_TRUE(fs.exists("/logs/20250820.csv"));    // today's file created
}

static void test_seed_fills_whole_days_and_leaves_the_live_file_alone() {
    FakeStore fs;
    fs.capacity = 5000000;
    Logger log(fs);
    Config cfg;
    log.begin(0);

    const int rows = log.seed(cfg, 3, kEpoch);
    TEST_ASSERT_EQUAL_INT(180, rows);   // 3 h at 60 s spacing

    const auto files = log.files();
    TEST_ASSERT_TRUE(files.size() >= 1);
    TEST_ASSERT_EQUAL_STRING("", log.currentPath().c_str());   // reopened by the next row
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_day_stamp_uses_the_local_timezone);
    RUN_TEST(test_row_matches_the_documented_schema);
    RUN_TEST(test_invalid_sensors_leave_blank_cells_not_zeros);
    RUN_TEST(test_header_written_once_then_rows_appended);
    RUN_TEST(test_unknown_time_logs_negative_uptime_then_switches_file);
    RUN_TEST(test_tick_respects_the_log_interval);
    RUN_TEST(test_rotation_deletes_the_oldest_file_first);
    RUN_TEST(test_seed_fills_whole_days_and_leaves_the_live_file_alone);
    return UNITY_END();
}
