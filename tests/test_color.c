#include "test_helpers.h"
#include "utils/color.h"

#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>

// Verify that color escape sequences produce correct ANSI codes
static void test_color_escape_sequences_match_expected_values(void)
{
    CU_ASSERT_STRING_EQUAL(COLOR_RED, "\033[31m");
    CU_ASSERT_STRING_EQUAL(COLOR_GREEN, "\033[32m");
    CU_ASSERT_STRING_EQUAL(COLOR_YELLOW, "\033[33m");
    CU_ASSERT_STRING_EQUAL(COLOR_RESET, "\033[0m");
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    hpp_test_disable_logging();

    CU_pSuite suite = CU_add_suite("Color", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite,
                    "color escape sequences match expected values",
                    test_color_escape_sequences_match_expected_values) == NULL)
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
