#include "test_helpers.h"
#include "utils/timing.h"

#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <math.h>
#include <stdint.h>

// Verify that timing constants (conversions) are internally consistent
static void test_timing_constants_are_consistent(void)
{
    CU_ASSERT_EQUAL(TIMER_NS_IN_S, 1000000000ULL);
    CU_ASSERT_EQUAL(TIMER_US_IN_S, 1000000ULL);
    CU_ASSERT_EQUAL(TIMER_MS_IN_S, 1000ULL);
}

// Verify that timer measurements return non-negative values
static void test_timer_measurement_is_non_negative(void)
{
    hpp_timer timer = {0};

    timer_start(&timer);
    volatile uint64_t sink = 0;
    for (uint64_t idx = 0; idx < 100000U; ++idx)
    {
        sink += idx;
    }
    timer_stop(&timer);

    CU_ASSERT_TRUE(sink > 0U);
    CU_ASSERT_TRUE(timer_ns(&timer) >= 0U);
    CU_ASSERT_TRUE(timer_us(&timer) >= 0.0);
    CU_ASSERT_TRUE(timer_ms(&timer) >= 0.0);
    CU_ASSERT_TRUE(timer_s(&timer) >= 0.0);
}

#if !defined(_WIN32) && !defined(_WIN64) && !defined(__APPLE__)
static void test_timer_unit_conversions_match_known_interval(void)
{
    hpp_timer timer = {
        .start =
            {
                .tv_sec  = 1,
                .tv_nsec = 200,
            },
        .end =
            {
                .tv_sec  = 3,
                .tv_nsec = 700,
            },
    };

    const uint64_t expected_ns = (2ULL * TIMER_NS_IN_S) + 500ULL;

    CU_ASSERT_EQUAL(timer_ns(&timer), expected_ns);
    CU_ASSERT_DOUBLE_EQUAL(timer_us(&timer), (double)expected_ns / 1000.0, 0.0001);
    CU_ASSERT_DOUBLE_EQUAL(timer_ms(&timer), (double)expected_ns / 1000000.0, 0.0001);
    CU_ASSERT_DOUBLE_EQUAL(timer_s(&timer), (double)expected_ns / 1000000000.0, 0.0000001);
}
#endif

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    hpp_test_disable_logging();

    CU_pSuite suite = CU_add_suite("Timing", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite,
                    "timing constants are consistent",
                    test_timing_constants_are_consistent) == NULL ||
        CU_add_test(suite,
                    "timer measurement is non negative",
                    test_timer_measurement_is_non_negative) == NULL
#if !defined(_WIN32) && !defined(_WIN64) && !defined(__APPLE__)
        || CU_add_test(suite,
                       "timer unit conversions match known interval",
                       test_timer_unit_conversions_match_known_interval) == NULL
#endif
    )
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    const unsigned int failures = CU_get_number_of_failures();
    CU_cleanup_registry();

    return (failures == 0U) ? 0 : 1;
}
