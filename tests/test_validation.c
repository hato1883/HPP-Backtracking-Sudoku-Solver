#include "solver/validation.h"
#include "test_helpers.h"

#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>

// Verify that constraint objects can be properly allocated and freed
static void test_constraints_create_and_destroy(void)
{
    hpp_validation_constraints* constraints = hpp_validation_constraints_create(9, 3);
    CU_ASSERT_PTR_NOT_NULL(constraints);

    if (constraints != NULL)
    {
        CU_ASSERT_EQUAL(constraints->side_length, 9);
        CU_ASSERT_EQUAL(constraints->block_length, 3);
        CU_ASSERT_PTR_NOT_NULL(constraints->row_bitvectors);
        CU_ASSERT_PTR_NOT_NULL(constraints->col_bitvectors);
        CU_ASSERT_PTR_NOT_NULL(constraints->box_bitvectors);
    }

    hpp_validation_constraints_destroy(&constraints);
    CU_ASSERT_PTR_NULL(constraints);
}

static void test_init_from_board_populates_row_col_box_bits(void)
{
    // clang-format off
    const hpp_cell cells[] = {
            1, 0, 0, 4,
            0, 4, 0, 0,
            0, 0, 4, 0,
            4, 0, 0, 1,
    };
    // clang-format on

    hpp_board* board = hpp_test_create_board_from_cells(cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    hpp_validation_constraints* constraints =
        hpp_validation_constraints_create(board->side_length, board->block_length);
    if (constraints == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate constraints");
        destroy_board(&board);
        return;
    }

    CU_ASSERT_TRUE(hpp_validation_constraints_init_from_board(constraints, board));

    CU_ASSERT_TRUE(hpp_validation_row_has_value(constraints, 0, 1));
    CU_ASSERT_TRUE(hpp_validation_col_has_value(constraints, 0, 1));
    CU_ASSERT_TRUE(hpp_validation_box_has_value(constraints, 0, 0, 1));

    CU_ASSERT_TRUE(hpp_validation_row_has_value(constraints, 0, 4));
    CU_ASSERT_TRUE(hpp_validation_col_has_value(constraints, 3, 4));
    CU_ASSERT_TRUE(hpp_validation_box_has_value(constraints, 1, 1, 4));

    hpp_validation_constraints_destroy(&constraints);
    destroy_board(&board);
}

static void test_init_from_board_rejects_row_duplicates(void)
{
    // clang-format off
    const hpp_cell invalid_cells[] = {
            1, 1, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
    };
    // clang-format on

    hpp_board* board = hpp_test_create_board_from_cells(invalid_cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    hpp_validation_constraints* constraints =
        hpp_validation_constraints_create(board->side_length, board->block_length);
    if (constraints == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate constraints");
        destroy_board(&board);
        return;
    }

    CU_ASSERT_FALSE(hpp_validation_constraints_init_from_board(constraints, board));

    hpp_validation_constraints_destroy(&constraints);
    destroy_board(&board);
}

static void test_init_from_board_rejects_shape_mismatch(void)
{
    hpp_board* board = create_board(4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    hpp_validation_constraints* constraints = hpp_validation_constraints_create(9, 3);
    if (constraints == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate constraints");
        destroy_board(&board);
        return;
    }

    CU_ASSERT_FALSE(hpp_validation_constraints_init_from_board(constraints, board));

    hpp_validation_constraints_destroy(&constraints);
    destroy_board(&board);
}

static void test_row_col_box_add_remove_round_trip(void)
{
    hpp_validation_constraints* constraints = hpp_validation_constraints_create(4, 2);
    if (constraints == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate constraints");
        return;
    }

    CU_ASSERT_TRUE(hpp_validation_row_add_value(constraints, 0, 1));
    CU_ASSERT_FALSE(hpp_validation_row_add_value(constraints, 0, 1));
    CU_ASSERT_TRUE(hpp_validation_row_has_value(constraints, 0, 1));
    hpp_validation_row_remove_value(constraints, 0, 1);
    CU_ASSERT_FALSE(hpp_validation_row_has_value(constraints, 0, 1));

    CU_ASSERT_TRUE(hpp_validation_col_add_value(constraints, 1, 3));
    CU_ASSERT_FALSE(hpp_validation_col_add_value(constraints, 1, 3));
    CU_ASSERT_TRUE(hpp_validation_col_has_value(constraints, 1, 3));
    hpp_validation_col_remove_value(constraints, 1, 3);
    CU_ASSERT_FALSE(hpp_validation_col_has_value(constraints, 1, 3));

    CU_ASSERT_TRUE(hpp_validation_box_add_value(constraints, 2, 3, 4));
    CU_ASSERT_FALSE(hpp_validation_box_add_value(constraints, 2, 3, 4));
    CU_ASSERT_TRUE(hpp_validation_box_has_value(constraints, 2, 3, 4));
    hpp_validation_box_remove_value(constraints, 2, 3, 4);
    CU_ASSERT_FALSE(hpp_validation_box_has_value(constraints, 2, 3, 4));

    hpp_validation_constraints_destroy(&constraints);
}

static void test_can_place_value_checks_all_units(void)
{
    hpp_validation_constraints* constraints = hpp_validation_constraints_create(4, 2);
    if (constraints == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate constraints");
        return;
    }

    CU_ASSERT_TRUE(hpp_validation_row_add_value(constraints, 0, 1));
    CU_ASSERT_TRUE(hpp_validation_col_add_value(constraints, 2, 2));
    CU_ASSERT_TRUE(hpp_validation_box_add_value(constraints, 1, 1, 3));

    CU_ASSERT_FALSE(hpp_validation_can_place_value(constraints, 0, 3, 1));
    CU_ASSERT_FALSE(hpp_validation_can_place_value(constraints, 3, 2, 2));
    CU_ASSERT_FALSE(hpp_validation_can_place_value(constraints, 0, 0, 3));
    CU_ASSERT_TRUE(hpp_validation_can_place_value(constraints, 3, 3, 4));

    hpp_validation_constraints_destroy(&constraints);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    hpp_test_disable_logging();

    CU_pSuite suite = CU_add_suite("Validation", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite, "constraints create and destroy", test_constraints_create_and_destroy) ==
            NULL ||
        CU_add_test(suite,
                    "init from board populates row/col/box bits",
                    test_init_from_board_populates_row_col_box_bits) == NULL ||
        CU_add_test(suite,
                    "init from board rejects row duplicates",
                    test_init_from_board_rejects_row_duplicates) == NULL ||
        CU_add_test(suite,
                    "init from board rejects shape mismatch",
                    test_init_from_board_rejects_shape_mismatch) == NULL ||
        CU_add_test(suite,
                    "row/col/box add remove round trip",
                    test_row_col_box_add_remove_round_trip) == NULL ||
        CU_add_test(suite,
                    "can place value checks all units",
                    test_can_place_value_checks_all_units) == NULL)
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
