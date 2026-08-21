// Every fault trip, the latch, and the reset.
#include <unity.h>

#include "core/Safety.h"
#include "core/SensorHub.h"

using namespace dh;

// Unity compares integers; enum class does not implicitly convert.
#define ASSERT_ENUM(exp, act) TEST_ASSERT_EQUAL_INT(static_cast<int>(exp), static_cast<int>(act))

namespace {

Config cfg;

Reading ok(float t, float rh) {
    Reading r;
    r.t = t;
    r.rh = rh;
    r.valid = true;
    return r;
}
Reading dead() { return Reading{}; }

// Feed the hub enough identical samples that the median+EMA filter has settled.
void feed(SensorHub& hub, const Reading& bot, const Reading& top, uint32_t& t, int n = 12) {
    for (int i = 0; i < n; i++) {
        t += 1000;
        hub.update(bot, top, cfg, t);
    }
}

}  // namespace

void setUp() { cfg = Config{}; }
void tearDown() {}

static void test_healthy_system_does_not_trip() {
    SensorHub hub;
    Safety s;
    uint32_t t = 1000;
    hub.begin(t);
    s.begin(t, false);

    for (int i = 0; i < 100; i++) {
        feed(hub, ok(43.0f, 40.0f), ok(40.0f, 70.0f), t, 1);
        s.evaluate(hub, false, false, t);
    }
    TEST_ASSERT_FALSE(s.latched());
    ASSERT_ENUM(Fault::None, s.fault());
}

static void test_overtemp_on_either_sensor_latches() {
    for (int which = 0; which < 2; which++) {
        SensorHub hub;
        Safety s;
        uint32_t t = 1000;
        hub.begin(t);
        s.begin(t, false);

        const Reading hot = ok(70.0f, 20.0f);
        const Reading cool = ok(40.0f, 60.0f);
        feed(hub, which == 0 ? hot : cool, which == 0 ? cool : hot, t);
        s.evaluate(hub, false, true, t);

        TEST_ASSERT_TRUE(s.latched());
        ASSERT_ENUM(Fault::OverTemp, s.fault());
    }
}

static void test_overtemp_boundary_is_65() {
    SensorHub hub;
    Safety s;
    uint32_t t = 1000;
    hub.begin(t);
    s.begin(t, false);

    feed(hub, ok(64.0f, 20.0f), ok(60.0f, 30.0f), t);
    s.evaluate(hub, false, true, t);
    TEST_ASSERT_FALSE(s.latched());

    feed(hub, ok(65.5f, 20.0f), ok(60.0f, 30.0f), t);
    s.evaluate(hub, false, true, t);
    TEST_ASSERT_TRUE(s.latched());
}

static void test_one_dead_sensor_is_not_a_fault() {
    SensorHub hub;
    Safety s;
    uint32_t t = 1000;
    hub.begin(t);
    s.begin(t, false);

    for (int i = 0; i < 60; i++) {
        feed(hub, dead(), ok(41.0f, 70.0f), t, 1);
        s.evaluate(hub, false, false, t);
    }
    TEST_ASSERT_FALSE(s.latched());
    // Control falls back to the top sensor.
    TEST_ASSERT_TRUE(hub.control().valid);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 41.0f, hub.control().t);
}

static void test_both_sensors_dead_trips_after_10s() {
    SensorHub hub;
    Safety s;
    uint32_t t = 1000;
    hub.begin(t);
    s.begin(t, false);

    feed(hub, ok(40.0f, 50.0f), ok(38.0f, 60.0f), t);
    s.evaluate(hub, false, false, t);
    TEST_ASSERT_FALSE(s.latched());

    // 9 s of darkness: still inside the grace window.
    for (int i = 0; i < 9; i++) {
        t += 1000;
        hub.update(dead(), dead(), cfg, t);
        s.evaluate(hub, false, false, t);
    }
    TEST_ASSERT_FALSE(s.latched());

    for (int i = 0; i < 3; i++) {
        t += 1000;
        hub.update(dead(), dead(), cfg, t);
        s.evaluate(hub, false, false, t);
    }
    TEST_ASSERT_TRUE(s.latched());
    ASSERT_ENUM(Fault::SensorTimeout, s.fault());
}

static void test_klixon_open_latches_immediately() {
    SensorHub hub;
    Safety s;
    uint32_t t = 1000;
    hub.begin(t);
    s.begin(t, false);

    feed(hub, ok(40.0f, 50.0f), ok(38.0f, 60.0f), t);
    s.evaluate(hub, true, false, t);
    TEST_ASSERT_TRUE(s.latched());
    ASSERT_ENUM(Fault::Klixon, s.fault());
}

static void test_heater_that_does_not_heat_trips_after_10min() {
    SensorHub hub;
    Safety s;
    uint32_t t = 1000;
    hub.begin(t);
    s.begin(t, false);

    // Heater commanded on, temperature pinned at 30 C.
    for (int i = 0; i < 9 * 60; i++) {
        t += 1000;
        hub.update(ok(30.0f, 40.0f), ok(28.0f, 50.0f), cfg, t);
        s.evaluate(hub, false, true, t);
    }
    TEST_ASSERT_FALSE(s.latched());   // 9 minutes: still allowed

    for (int i = 0; i < 90; i++) {
        t += 1000;
        hub.update(ok(30.0f, 40.0f), ok(28.0f, 50.0f), cfg, t);
        s.evaluate(hub, false, true, t);
    }
    TEST_ASSERT_TRUE(s.latched());
    ASSERT_ENUM(Fault::HeaterIneffective, s.fault());
}

static void test_heater_that_does_heat_does_not_trip() {
    SensorHub hub;
    Safety s;
    uint32_t t = 1000;
    hub.begin(t);
    s.begin(t, false);

    float temp = 22.0f;
    for (int i = 0; i < 30 * 60; i++) {
        t += 1000;
        temp += 0.01f;   // 0.6 C/min — a real element
        if (temp > 60.0f) temp = 60.0f;
        hub.update(ok(temp, 30.0f), ok(temp - 2.0f, 40.0f), cfg, t);
        s.evaluate(hub, false, true, t);
        if (s.latched()) break;
    }
    TEST_ASSERT_FALSE(s.latched());
}

static void test_heater_check_rearms_when_heater_cycles_off() {
    SensorHub hub;
    Safety s;
    uint32_t t = 1000;
    hub.begin(t);
    s.begin(t, false);

    for (int cycle = 0; cycle < 3; cycle++) {
        for (int i = 0; i < 8 * 60; i++) {   // 8 min on, no rise
            t += 1000;
            hub.update(ok(30.0f, 40.0f), ok(28.0f, 50.0f), cfg, t);
            s.evaluate(hub, false, true, t);
        }
        t += 1000;   // then off — the detector must disarm
        hub.update(ok(30.0f, 40.0f), ok(28.0f, 50.0f), cfg, t);
        s.evaluate(hub, false, false, t);
    }
    TEST_ASSERT_FALSE(s.latched());
}

static void test_latch_keeps_the_first_cause_and_survives_recovery() {
    SensorHub hub;
    Safety s;
    uint32_t t = 1000;
    hub.begin(t);
    s.begin(t, false);

    feed(hub, ok(70.0f, 20.0f), ok(60.0f, 30.0f), t);
    s.evaluate(hub, false, false, t);
    ASSERT_ENUM(Fault::OverTemp, s.fault());

    // Cabinet cools down and the klixon also opens: the original cause must stand.
    feed(hub, ok(30.0f, 40.0f), ok(28.0f, 50.0f), t);
    s.evaluate(hub, true, false, t);
    TEST_ASSERT_TRUE(s.latched());
    ASSERT_ENUM(Fault::OverTemp, s.fault());
}

static void test_reset_clears_the_latch() {
    SensorHub hub;
    Safety s;
    uint32_t t = 1000;
    hub.begin(t);
    s.begin(t, false);

    feed(hub, ok(70.0f, 20.0f), ok(60.0f, 30.0f), t);
    s.evaluate(hub, false, false, t);
    TEST_ASSERT_TRUE(s.latched());

    s.reset(t);
    TEST_ASSERT_FALSE(s.latched());
    ASSERT_ENUM(Fault::None, s.fault());

    // Still hot, so the very next evaluation trips again — reset is not a bypass.
    s.evaluate(hub, false, false, t);
    TEST_ASSERT_TRUE(s.latched());
}

static void test_watchdog_reset_boots_latched() {
    Safety s;
    s.begin(1000, true);
    TEST_ASSERT_TRUE(s.latched());
    ASSERT_ENUM(Fault::WatchdogReset, s.fault());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_healthy_system_does_not_trip);
    RUN_TEST(test_overtemp_on_either_sensor_latches);
    RUN_TEST(test_overtemp_boundary_is_65);
    RUN_TEST(test_one_dead_sensor_is_not_a_fault);
    RUN_TEST(test_both_sensors_dead_trips_after_10s);
    RUN_TEST(test_klixon_open_latches_immediately);
    RUN_TEST(test_heater_that_does_not_heat_trips_after_10min);
    RUN_TEST(test_heater_that_does_heat_does_not_trip);
    RUN_TEST(test_heater_check_rearms_when_heater_cycles_off);
    RUN_TEST(test_latch_keeps_the_first_cause_and_survives_recovery);
    RUN_TEST(test_reset_clears_the_latch);
    RUN_TEST(test_watchdog_reset_boots_latched);
    return UNITY_END();
}
