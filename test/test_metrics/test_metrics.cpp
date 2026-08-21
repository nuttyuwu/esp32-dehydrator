// Psychrometrics against hand-computed / textbook values.
#include <unity.h>

#include "core/Metrics.h"

using namespace dh::metrics;

void setUp() {}
void tearDown() {}

static void test_psat_reference_points() {
    // Magnus at 0 C is the formula's own constant.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.61078f, psatKPa(0.0f));
    // Steam tables: 2.339 kPa at 20 C, 9.59 kPa at 45 C, ~101.3 kPa at 100 C.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.339f, psatKPa(20.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 9.59f, psatKPa(45.0f));
    TEST_ASSERT_FLOAT_WITHIN(1.5f, 101.3f, psatKPa(100.0f));
}

static void test_psat_monotonic() {
    float prev = psatKPa(-10.0f);
    for (float t = -9.0f; t <= 80.0f; t += 1.0f) {
        const float p = psatKPa(t);
        TEST_ASSERT_TRUE(p > prev);
        prev = p;
    }
}

static void test_absolute_humidity() {
    // Saturated air at 20 C holds 17.3 g/m3.
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 17.3f, absHumidity(20.0f, 100.0f));
    // Half the relative humidity is half the water.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, absHumidity(20.0f, 100.0f) / 2.0f,
                             absHumidity(20.0f, 50.0f));
    // Cabinet conditions: 44 C at 50 %RH.
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 31.1f, absHumidity(44.0f, 50.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, absHumidity(30.0f, 0.0f));
}

static void test_ah_clamps_out_of_range_rh() {
    TEST_ASSERT_EQUAL_FLOAT(absHumidity(25.0f, 100.0f), absHumidity(25.0f, 140.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, absHumidity(25.0f, -5.0f));
}

static void test_vpd() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.169f, vpdKPa(20.0f, 50.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, vpdKPa(20.0f, 100.0f));
}

static void test_dew_point() {
    TEST_ASSERT_FLOAT_WITHIN(0.15f, 9.27f, dewPointC(20.0f, 50.0f));
    // At saturation the dew point is the air temperature.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 30.0f, dewPointC(30.0f, 100.0f));
}

// The drying signal from ARCHITECTURE.md section 9: warm dry air in at the bottom, cooler
// wet air out at the top, and the top must carry MORE water per cubic metre.
static void test_moisture_removal_is_positive_while_drying() {
    const float ahBottom = absHumidity(44.0f, 12.0f);
    const float ahTop = absHumidity(41.0f, 74.0f);
    TEST_ASSERT_TRUE(ahTop - ahBottom > 5.0f);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_psat_reference_points);
    RUN_TEST(test_psat_monotonic);
    RUN_TEST(test_absolute_humidity);
    RUN_TEST(test_ah_clamps_out_of_range_rh);
    RUN_TEST(test_vpd);
    RUN_TEST(test_dew_point);
    RUN_TEST(test_moisture_removal_is_positive_while_drying);
    return UNITY_END();
}
