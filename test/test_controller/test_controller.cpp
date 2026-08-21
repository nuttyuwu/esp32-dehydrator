// Hysteresis band, minimum on/off dwell, and the heater/fan interlock.
#include <unity.h>

#include "core/Controller.h"
#include "hal/IActuators.h"

using namespace dh;

namespace {

Config cfg;   // defaults: 44.0 C, 1.0 C band, 30 s / 30 s

// A concrete ActuatorBase that records what it was told to write, so the interlock in the
// base class is what is under test.
class FakeActuators : public ActuatorBase {
  public:
    bool begin() override { return true; }
    bool klixonOpen() override { return false; }
    Outputs written;
    int changes = 0;

  protected:
    void write(const Outputs& o) override { written = o; }
    void onChange(const Outputs&) override { changes++; }
};

}  // namespace

void setUp() { cfg = Config{}; }
void tearDown() {}

static void test_thresholds_follow_setpoint_and_band() {
    Controller c;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 43.5f, c.onThreshold(cfg));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 44.5f, c.offThreshold(cfg));

    cfg.setpointC = 50.0f;
    cfg.hysteresisC = 2.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 49.0f, c.onThreshold(cfg));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 51.0f, c.offThreshold(cfg));
}

static void test_turns_on_below_band_and_off_above() {
    Controller c;
    uint32_t t = 1000;
    c.begin(t);

    TEST_ASSERT_TRUE(c.update(cfg, 22.0f, true, true, t));   // cold: on immediately
    t += 60000;
    TEST_ASSERT_TRUE(c.update(cfg, 44.0f, true, true, t));   // inside the band: stays on
    t += 1000;
    TEST_ASSERT_TRUE(c.update(cfg, 44.4f, true, true, t));   // still under the off point
    t += 1000;
    TEST_ASSERT_FALSE(c.update(cfg, 44.5f, true, true, t));  // at the off point: off
    t += 60000;
    TEST_ASSERT_FALSE(c.update(cfg, 43.6f, true, true, t));  // inside the band: stays off
    t += 1000;
    TEST_ASSERT_TRUE(c.update(cfg, 43.5f, true, true, t));   // at the on point: on
}

static void test_min_on_time_blocks_early_off() {
    Controller c;
    uint32_t t = 1000;
    c.begin(t);
    TEST_ASSERT_TRUE(c.update(cfg, 20.0f, true, true, t));

    t += 10000;   // 10 s < 30 s
    TEST_ASSERT_TRUE(c.update(cfg, 60.0f, true, true, t));
    t += 15000;   // 25 s
    TEST_ASSERT_TRUE(c.update(cfg, 60.0f, true, true, t));
    t += 6000;    // 31 s — dwell satisfied
    TEST_ASSERT_FALSE(c.update(cfg, 60.0f, true, true, t));
}

static void test_min_off_time_blocks_early_on() {
    Controller c;
    uint32_t t = 1000;
    c.begin(t);
    c.update(cfg, 20.0f, true, true, t);       // on
    t += 40000;
    c.update(cfg, 50.0f, true, true, t);       // off
    TEST_ASSERT_FALSE(c.heater());

    t += 5000;
    TEST_ASSERT_FALSE(c.update(cfg, 20.0f, true, true, t));   // freezing, but dwell holds
    t += 20000;
    TEST_ASSERT_FALSE(c.update(cfg, 20.0f, true, true, t));   // 25 s
    t += 6000;
    TEST_ASSERT_TRUE(c.update(cfg, 20.0f, true, true, t));    // 31 s
}

static void test_disallowing_heat_overrides_min_on() {
    Controller c;
    uint32_t t = 1000;
    c.begin(t);
    TEST_ASSERT_TRUE(c.update(cfg, 20.0f, true, true, t));

    t += 1000;   // well inside minOn — safety must win anyway
    TEST_ASSERT_FALSE(c.update(cfg, 20.0f, true, false, t));
}

static void test_invalid_sensor_turns_heater_off() {
    Controller c;
    uint32_t t = 1000;
    c.begin(t);
    TEST_ASSERT_TRUE(c.update(cfg, 20.0f, true, true, t));
    t += 40000;
    TEST_ASSERT_FALSE(c.update(cfg, 20.0f, false, true, t));
}

static void test_purge_window_runs_60s_after_heater_off() {
    Controller c;
    uint32_t t = 1000;
    c.begin(t);
    TEST_ASSERT_FALSE(c.inPurge(t));   // nothing to purge before the first cycle

    c.update(cfg, 20.0f, true, true, t);
    t += 40000;
    c.update(cfg, 50.0f, true, true, t);   // off here
    TEST_ASSERT_TRUE(c.inPurge(t));
    TEST_ASSERT_TRUE(c.inPurge(t + 59000));
    TEST_ASSERT_FALSE(c.inPurge(t + 61000));
}

static void test_actuator_interlock_blocks_heater_without_stack_fan() {
    FakeActuators a;
    a.begin();

    Outputs want;
    want.heater = true;
    want.fanStack = false;
    a.apply(want);
    TEST_ASSERT_FALSE(a.written.heater);
    TEST_ASSERT_FALSE(a.current().heater);

    want.fanStack = true;
    a.apply(want);
    TEST_ASSERT_TRUE(a.written.heater);
    TEST_ASSERT_TRUE(a.current().heater);
}

static void test_actuator_reports_changes_once() {
    FakeActuators a;
    a.begin();
    Outputs o;
    o.fanStack = true;
    a.apply(o);
    a.apply(o);
    a.apply(o);
    TEST_ASSERT_EQUAL_INT(1, a.changes);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_thresholds_follow_setpoint_and_band);
    RUN_TEST(test_turns_on_below_band_and_off_above);
    RUN_TEST(test_min_on_time_blocks_early_off);
    RUN_TEST(test_min_off_time_blocks_early_on);
    RUN_TEST(test_disallowing_heat_overrides_min_on);
    RUN_TEST(test_invalid_sensor_turns_heater_off);
    RUN_TEST(test_purge_window_runs_60s_after_heater_off);
    RUN_TEST(test_actuator_interlock_blocks_heater_without_stack_fan);
    RUN_TEST(test_actuator_reports_changes_once);
    return UNITY_END();
}
