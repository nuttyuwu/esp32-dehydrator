// Closed-loop tests: the real state machine driving the real plant model.
// These cover acceptance criteria 1-5 and 7 from BUILD_PROMPT.md without any hardware.
#include <unity.h>

#include "core/SensorHub.h"
#include "core/StateMachine.h"
#include "hal/IActuators.h"
#include "sim/Plant.h"

using namespace dh;

// Unity compares integers; enum class does not implicitly convert.
#define ASSERT_ENUM(exp, act) TEST_ASSERT_EQUAL_INT(static_cast<int>(exp), static_cast<int>(act))

namespace {

// One tick of the whole loop: plant -> sensors -> hub -> state machine -> interlock.
struct Rig {
    Config cfg;
    sim::Plant plant;
    SensorHub hub;
    StateMachine sm;
    Outputs out;
    uint32_t t = 1000;

    bool botDead = false;
    bool topDead = false;
    bool klixon = false;
    bool overrideBot = false;
    float botValue = 0.0f;

    void begin() {
        plant.setAmbient(22.0f);
        plant.reset();
        hub.begin(t);
        sm.begin(t, false);
    }

    void step() {
        plant.setOutputs(out);
        plant.step(1.0f);

        Reading b;
        b.t = overrideBot ? botValue : plant.tBot();
        b.rh = plant.rhBot();
        b.valid = !botDead;

        Reading tp;
        tp.t = plant.tTop();
        tp.rh = plant.rhTop();
        tp.valid = !topDead;

        t += 1000;
        hub.update(b, tp, cfg, t);

        TickInputs in;
        in.nowMs = t;
        in.klixonOpen = klixon;
        out = sm.tick(cfg, hub, in);

        // Same interlock the actuator layer applies on the device.
        if (out.heater && !out.fanStack) out.heater = false;
    }

    void run(int n) {
        for (int i = 0; i < n; i++) step();
    }

    bool runUntil(State want, int maxSteps) {
        for (int i = 0; i < maxSteps; i++) {
            step();
            if (sm.state() == want) return true;
        }
        return false;
    }
};

}  // namespace

void setUp() {}
void tearDown() {}

// Acceptance 1: cold boot is IDLE with everything off.
static void test_cold_boot_is_idle_and_quiet() {
    Rig r;
    r.begin();
    ASSERT_ENUM(State::Idle, r.sm.state());

    r.run(120);
    ASSERT_ENUM(State::Idle, r.sm.state());
    TEST_ASSERT_FALSE(r.out.heater);
    TEST_ASSERT_FALSE(r.out.fanIntake);
    TEST_ASSERT_FALSE(r.out.fanStack);
    TEST_ASSERT_FALSE(r.sm.faultLatched());
}

// Acceptance 2: PREHEAT reaches the band, hands over to DRYING, and the heater cycles
// inside the hysteresis window instead of chattering.
static void test_preheat_to_drying_then_cycles_in_band() {
    Rig r;
    r.begin();
    r.sm.start("test", r.t, 1755660000, 0);
    ASSERT_ENUM(State::Preheat, r.sm.state());

    TEST_ASSERT_TRUE(r.runUntil(State::Drying, 3000));
    TEST_ASSERT_TRUE(r.out.fanStack);   // stack fan runs whenever we are heating

    // Two hours of steady drying: count heater transitions and watch the envelope.
    int transitions = 0;
    bool prev = r.out.heater;
    float lo = 999.0f, hi = -999.0f;
    uint32_t lastChange = r.t;
    uint32_t shortestDwell = 0xFFFFFFFF;

    for (int i = 0; i < 7200; i++) {
        r.step();
        if (r.out.heater != prev) {
            transitions++;
            const uint32_t dwell = r.t - lastChange;
            if (dwell < shortestDwell) shortestDwell = dwell;
            lastChange = r.t;
            prev = r.out.heater;
        }
        const float tc = r.hub.control().t;
        if (tc < lo) lo = tc;
        if (tc > hi) hi = tc;
    }

    TEST_ASSERT_TRUE(transitions > 2);          // it really is cycling
    TEST_ASSERT_TRUE(shortestDwell >= 30000);   // and never faster than the dwell timers
    // The model cabinet has a ~200 s time constant, so the mandatory 30 s off-dwell costs
    // about 3 C of undershoot. A real cabinet full of food has far more thermal mass and
    // will sit much tighter — see README.md.
    TEST_ASSERT_TRUE(lo > 40.0f);
    TEST_ASSERT_TRUE(hi < 47.0f);
    TEST_ASSERT_FALSE(r.sm.faultLatched());
}

// Acceptance 3: manual heater with no stack fan must not energise the element.
static void test_manual_heater_without_stack_fan_stays_off() {
    Rig r;
    r.begin();
    r.sm.setMode(Mode::Manual, r.t);

    ManualRequest m;
    m.heater = true;
    m.fanStack = false;
    r.sm.setManual(m);
    r.run(10);
    ASSERT_ENUM(State::Manual, r.sm.state());
    TEST_ASSERT_FALSE(r.out.heater);

    m.fanStack = true;
    r.sm.setManual(m);
    r.run(3);
    TEST_ASSERT_TRUE(r.out.heater);
}

// Acceptance 4: 70 C on the bottom sensor latches, kills the heater, keeps fans running,
// and only an explicit reset clears it.
static void test_overtemp_latches_and_only_reset_clears_it() {
    Rig r;
    r.begin();
    r.sm.start("test", r.t, 1755660000, 0);
    r.runUntil(State::Drying, 3000);

    r.overrideBot = true;
    r.botValue = 70.0f;
    r.run(3);

    ASSERT_ENUM(State::Fault, r.sm.state());
    ASSERT_ENUM(Fault::OverTemp, r.sm.fault());
    TEST_ASSERT_FALSE(r.out.heater);
    TEST_ASSERT_TRUE(r.out.fanStack);   // still hot: purge

    // Sensor recovers, but the latch holds.
    r.overrideBot = false;
    r.run(60);
    ASSERT_ENUM(State::Fault, r.sm.state());

    r.sm.resetFault(r.t);
    r.run(2);
    ASSERT_ENUM(State::Idle, r.sm.state());
    TEST_ASSERT_FALSE(r.sm.faultLatched());
}

// Acceptance 5: one dead sensor is a degraded run, not a fault; both is a fault.
static void test_sensor_fallback_then_timeout() {
    Rig r;
    r.begin();
    r.sm.start("test", r.t, 1755660000, 0);
    r.runUntil(State::Drying, 3000);

    r.botDead = true;
    r.run(60);
    TEST_ASSERT_FALSE(r.sm.faultLatched());
    ASSERT_ENUM(State::Drying, r.sm.state());
    TEST_ASSERT_TRUE(r.hub.control().valid);   // top sensor is carrying the loop

    r.topDead = true;
    r.run(15);
    ASSERT_ENUM(State::Fault, r.sm.state());
    ASSERT_ENUM(Fault::SensorTimeout, r.sm.fault());
    TEST_ASSERT_FALSE(r.out.heater);
}

static void test_klixon_open_latches() {
    Rig r;
    r.begin();
    r.sm.start("test", r.t, 1755660000, 0);
    r.run(30);

    r.klixon = true;
    r.run(2);
    ASSERT_ENUM(State::Fault, r.sm.state());
    ASSERT_ENUM(Fault::Klixon, r.sm.fault());
    TEST_ASSERT_FALSE(r.out.heater);
}

// Acceptance 7: a run that dries out finishes by itself — RH below target, held, then
// COOLDOWN and DONE. Starting at 35 % moisture keeps the test to a few seconds.
static void test_run_finishes_by_itself() {
    Rig r;
    r.begin();
    r.plant.setMoisture(0.35f);
    r.sm.start("carrot", r.t, 1755660000, 0);

    TEST_ASSERT_TRUE(r.runUntil(State::Cooldown, 20000));
    TEST_ASSERT_FALSE(r.out.heater);
    TEST_ASSERT_TRUE(r.out.fanStack);

    TEST_ASSERT_TRUE(r.runUntil(State::Done, 3000));
    TEST_ASSERT_FALSE(r.out.heater);
    TEST_ASSERT_FALSE(r.sm.session().active);
    TEST_ASSERT_TRUE(r.hub.control().t < 30.0f);
}

static void test_stop_goes_through_cooldown() {
    Rig r;
    r.begin();
    r.sm.start("test", r.t, 1755660000, 0);
    r.runUntil(State::Drying, 3000);

    r.sm.stop(r.t);
    r.run(2);
    ASSERT_ENUM(State::Cooldown, r.sm.state());
    TEST_ASSERT_FALSE(r.out.heater);
    TEST_ASSERT_TRUE(r.runUntil(State::Done, 3000));
}

static void test_manual_mode_times_out_after_30_min() {
    Rig r;
    r.begin();
    r.sm.setMode(Mode::Manual, r.t);
    ManualRequest m;
    m.fanStack = true;
    r.sm.setManual(m);

    r.run(29 * 60);
    ASSERT_ENUM(State::Manual, r.sm.state());
    r.run(2 * 60);
    ASSERT_ENUM(State::Idle, r.sm.state());
    TEST_ASSERT_FALSE(r.out.fanStack);
}

static void test_session_ids_increment() {
    Rig r;
    r.begin();
    r.sm.start("one", r.t, 1755660000, 0);
    const uint32_t first = r.sm.session().id;
    r.run(5);
    r.sm.stop(r.t);
    r.runUntil(State::Done, 3000);
    r.sm.start("two", r.t, 1755660000, 0);
    TEST_ASSERT_EQUAL_UINT32(first + 1, r.sm.session().id);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cold_boot_is_idle_and_quiet);
    RUN_TEST(test_preheat_to_drying_then_cycles_in_band);
    RUN_TEST(test_manual_heater_without_stack_fan_stays_off);
    RUN_TEST(test_overtemp_latches_and_only_reset_clears_it);
    RUN_TEST(test_sensor_fallback_then_timeout);
    RUN_TEST(test_klixon_open_latches);
    RUN_TEST(test_run_finishes_by_itself);
    RUN_TEST(test_stop_goes_through_cooldown);
    RUN_TEST(test_manual_mode_times_out_after_30_min);
    RUN_TEST(test_session_ids_increment);
    return UNITY_END();
}
