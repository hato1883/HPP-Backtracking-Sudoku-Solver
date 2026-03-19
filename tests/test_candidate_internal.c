#include "solver/candidate_internal.h"
#include "test_helpers.h"

#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <stdbool.h>
#include <stddef.h>

static bool init_state_from_cells(hpp_candidate_state* state, const hpp_cell* cells, size_t side)
{
    hpp_board* board = hpp_test_create_board_from_cells(cells, side);
    if (board == NULL)
    {
        return false;
    }

    const hpp_candidate_init_status status = hpp_candidate_state_init_from_board(state, board);
    destroy_board(&board);
    return status == HPP_CANDIDATE_INIT_OK;
}

static void drain_modified_stack(hpp_candidate_state* state)
{
    size_t cell_index = 0;
    while (hpp_candidate_modified_pop(state, &cell_index))
    {
    }
}

// Verify that candidate internal constants match bitvector bit count expectations
static void test_candidate_internal_constants_match_bitvector_domain(void)
{
    CU_ASSERT_EQUAL(HPP_CANDIDATE_VALUE_COUNT, HPP_BITVECTOR_VALUE_COUNT);
    CU_ASSERT_EQUAL(HPP_CANDIDATE_WORD_COUNT, HPP_BITVECTOR_WORD_COUNT);
    CU_ASSERT_EQUAL(HPP_CANDIDATE_CELL_BYTES, HPP_BITVECTOR_BYTES);
}

// Verify that cell accessor functions correctly map to storage layout
static void test_candidate_cell_accessors_match_storage_layout(void)
{
    // clang-format off
    const hpp_cell cells[] = {
            1, 0, 3, 0,
            0, 4, 0, 2,
            2, 0, 4, 0,
            0, 3, 0, 1,
    };
    // clang-format on

    hpp_candidate_state state = {0};
    if (!init_state_from_cells(&state, cells, 4))
    {
        CU_FAIL_FATAL("Failed to initialize candidate state");
        return;
    }

    hpp_bitvector_word*       cell0       = hpp_candidate_cell_at(&state, 0);
    hpp_bitvector_word*       cell1       = hpp_candidate_cell_at(&state, 1);
    const hpp_bitvector_word* cell1_const = hpp_candidate_cell_at_const(&state, 1);

    CU_ASSERT_EQUAL((size_t)(cell1 - cell0), HPP_CANDIDATE_WORD_COUNT);
    CU_ASSERT_PTR_EQUAL(cell1, (hpp_bitvector_word*)cell1_const);

    hpp_candidate_state_destroy(&state);
}

static void test_candidate_unit_bit_accessors_reflect_constraints(void)
{
    // clang-format off
    const hpp_cell cells[] = {
            1, 0, 3, 0,
            0, 4, 0, 2,
            2, 0, 4, 0,
            0, 3, 0, 1,
    };
    // clang-format on

    hpp_candidate_state state = {0};
    if (!init_state_from_cells(&state, cells, 4))
    {
        CU_FAIL_FATAL("Failed to initialize candidate state");
        return;
    }

    const hpp_bitvector_word* row0 = hpp_candidate_row_bits(state.constraints, 0);
    const hpp_bitvector_word* col0 = hpp_candidate_col_bits(state.constraints, 0);
    const hpp_bitvector_word* box0 = hpp_candidate_box_bits(state.constraints, 0);

    CU_ASSERT_TRUE(hpp_candidate_bit_test(row0, 1));
    CU_ASSERT_TRUE(hpp_candidate_bit_test(row0, 3));
    CU_ASSERT_TRUE(hpp_candidate_bit_test(col0, 1));
    CU_ASSERT_TRUE(hpp_candidate_bit_test(col0, 2));
    CU_ASSERT_TRUE(hpp_candidate_bit_test(box0, 1));
    CU_ASSERT_TRUE(hpp_candidate_bit_test(box0, 4));

    hpp_candidate_state_destroy(&state);
}

static void test_candidate_modified_stack_deduplicates_and_pops_lifo(void)
{
    // clang-format off
    const hpp_cell cells[] = {
            1, 0, 3, 0,
            0, 4, 0, 2,
            2, 0, 4, 0,
            0, 3, 0, 1,
    };
    // clang-format on

    hpp_candidate_state state = {0};
    if (!init_state_from_cells(&state, cells, 4))
    {
        CU_FAIL_FATAL("Failed to initialize candidate state");
        return;
    }

    drain_modified_stack(&state);
    CU_ASSERT_EQUAL(state.modified_count, 0);

    CU_ASSERT_TRUE(hpp_candidate_modified_push(&state, 5));
    CU_ASSERT_TRUE(hpp_candidate_modified_push(&state, 5));
    CU_ASSERT_TRUE(hpp_candidate_modified_push(&state, 2));
    CU_ASSERT_EQUAL(state.modified_count, 2);

    size_t popped = SIZE_MAX;
    CU_ASSERT_TRUE(hpp_candidate_modified_pop(&state, &popped));
    CU_ASSERT_EQUAL(popped, 2);
    CU_ASSERT_TRUE(hpp_candidate_modified_pop(&state, &popped));
    CU_ASSERT_EQUAL(popped, 5);
    CU_ASSERT_FALSE(hpp_candidate_modified_pop(&state, &popped));

    hpp_candidate_state_destroy(&state);
}

// Verify that AC-3 singleton selection prefers assigned values over domain hints
static void test_candidate_ac3_singleton_value_prefers_assignment_then_domain(void)
{
    // clang-format off
    const hpp_cell cells[] = {
            1, 2, 3, 0,
            3, 4, 1, 2,
            2, 1, 4, 3,
            4, 3, 2, 1,
    };
    // clang-format on

    hpp_candidate_state state = {0};
    if (!init_state_from_cells(&state, cells, 4))
    {
        CU_FAIL_FATAL("Failed to initialize candidate state");
        return;
    }

    CU_ASSERT_EQUAL(hpp_candidate_ac3_singleton_value_for_cell(&state, 0), 1);
    CU_ASSERT_EQUAL(state.candidate_counts[3], 1);
    CU_ASSERT_EQUAL(hpp_candidate_ac3_singleton_value_for_cell(&state, 3), 4);

    hpp_candidate_state_destroy(&state);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    hpp_test_disable_logging();

    CU_pSuite suite = CU_add_suite("Candidate Internal", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite,
                    "candidate internal constants match bitvector domain",
                    test_candidate_internal_constants_match_bitvector_domain) == NULL ||
        CU_add_test(suite,
                    "candidate cell accessors match storage layout",
                    test_candidate_cell_accessors_match_storage_layout) == NULL ||
        CU_add_test(suite,
                    "candidate unit bit accessors reflect constraints",
                    test_candidate_unit_bit_accessors_reflect_constraints) == NULL ||
        CU_add_test(suite,
                    "candidate modified stack deduplicates and pops LIFO",
                    test_candidate_modified_stack_deduplicates_and_pops_lifo) == NULL ||
        CU_add_test(suite,
                    "candidate ac3 singleton value prefers assignment then domain",
                    test_candidate_ac3_singleton_value_prefers_assignment_then_domain) == NULL)
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
