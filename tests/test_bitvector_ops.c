#include "data/bitvector_ops.h"
#include "test_helpers.h"

#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <stdbool.h>
#include <stdint.h>

// Verify that bitvector constants maintain internal consistency
static void test_bitvector_constants_are_consistent(void)
{
    CU_ASSERT_EQUAL(HPP_BITVECTOR_VALUE_COUNT, 256U);
    CU_ASSERT_EQUAL(HPP_BITVECTOR_WORD_BITS, sizeof(hpp_bitvector_word) * 8U);
    CU_ASSERT_EQUAL(HPP_BITVECTOR_WORD_COUNT * HPP_BITVECTOR_WORD_BITS, HPP_BITVECTOR_VALUE_COUNT);
    CU_ASSERT_EQUAL(HPP_BITVECTOR_BYTES, HPP_BITVECTOR_WORD_COUNT * sizeof(hpp_bitvector_word));
}

static void test_bitvector_position_maps_word_boundaries(void)
{
    size_t  word_idx   = 0;
    uint8_t bit_offset = 0;

    hpp_bitvector_position(0, &word_idx, &bit_offset);
    CU_ASSERT_EQUAL(word_idx, 0);
    CU_ASSERT_EQUAL(bit_offset, 0);

    hpp_bitvector_position(63, &word_idx, &bit_offset);
    CU_ASSERT_EQUAL(word_idx, 0);
    CU_ASSERT_EQUAL(bit_offset, 63);

    hpp_bitvector_position(64, &word_idx, &bit_offset);
    CU_ASSERT_EQUAL(word_idx, 1);
    CU_ASSERT_EQUAL(bit_offset, 0);

    hpp_bitvector_position(255, &word_idx, &bit_offset);
    CU_ASSERT_EQUAL(word_idx, HPP_BITVECTOR_WORD_COUNT - 1U);
    CU_ASSERT_EQUAL(bit_offset, 63);
}

static void test_bitvector_set_test_and_clear_across_words(void)
{
    hpp_bitvector_word bits[HPP_BITVECTOR_WORD_COUNT] = {0};

    CU_ASSERT_FALSE(hpp_bitvector_test(bits, 1));
    CU_ASSERT_FALSE(hpp_bitvector_test(bits, 64));
    CU_ASSERT_FALSE(hpp_bitvector_test(bits, 255));

    hpp_bitvector_set(bits, 1);
    hpp_bitvector_set(bits, 64);
    hpp_bitvector_set(bits, 255);

    CU_ASSERT_TRUE(hpp_bitvector_test(bits, 1));
    CU_ASSERT_TRUE(hpp_bitvector_test(bits, 64));
    CU_ASSERT_TRUE(hpp_bitvector_test(bits, 255));

    CU_ASSERT_FALSE(hpp_bitvector_test(bits, 2));
    CU_ASSERT_FALSE(hpp_bitvector_test(bits, 65));
    CU_ASSERT_FALSE(hpp_bitvector_test(bits, 254));

    hpp_bitvector_clear(bits, 64);
    CU_ASSERT_TRUE(hpp_bitvector_test(bits, 1));
    CU_ASSERT_FALSE(hpp_bitvector_test(bits, 64));
    CU_ASSERT_TRUE(hpp_bitvector_test(bits, 255));
}

static void test_bitvector_clear_on_unset_bit_is_idempotent(void)
{
    hpp_bitvector_word bits[HPP_BITVECTOR_WORD_COUNT] = {0};

    hpp_bitvector_clear(bits, 42);
    CU_ASSERT_FALSE(hpp_bitvector_test(bits, 42));

    hpp_bitvector_set(bits, 42);
    CU_ASSERT_TRUE(hpp_bitvector_test(bits, 42));

    hpp_bitvector_clear(bits, 42);
    hpp_bitvector_clear(bits, 42);
    CU_ASSERT_FALSE(hpp_bitvector_test(bits, 42));
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    hpp_test_disable_logging();

    CU_pSuite suite = CU_add_suite("Bitvector Ops", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite,
                    "bitvector constants are consistent",
                    test_bitvector_constants_are_consistent) == NULL ||
        CU_add_test(suite,
                    "bitvector position maps word boundaries",
                    test_bitvector_position_maps_word_boundaries) == NULL ||
        CU_add_test(suite,
                    "bitvector set test and clear across words",
                    test_bitvector_set_test_and_clear_across_words) == NULL ||
        CU_add_test(suite,
                    "bitvector clear on unset bit is idempotent",
                    test_bitvector_clear_on_unset_bit_is_idempotent) == NULL)
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
