#include "data/sudoku.h"
#include "test_helpers.h"

#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>

static void test_sudoku_create_and_destroy(void)
{
    hpp_sudoku* sudoku = hpp_sudoku_create(9, 3);
    CU_ASSERT_PTR_NOT_NULL(sudoku);

    if (sudoku != NULL)
    {
        CU_ASSERT_PTR_NOT_NULL(sudoku->board);
        CU_ASSERT_PTR_NOT_NULL(sudoku->constraints);
        CU_ASSERT_TRUE(hpp_sudoku_is_valid(sudoku));
    }

    hpp_sudoku_destroy(&sudoku);
    CU_ASSERT_PTR_NULL(sudoku);
}

static void test_sudoku_create_from_board_clones_state(void)
{
    // clang-format off
    const hpp_cell cells[] = {
            1, 0, 3, 0,
            0, 4, 0, 2,
            2, 0, 4, 0,
            0, 3, 0, 1,
    };
    // clang-format on

    hpp_board* source = hpp_test_create_board_from_cells(cells, 4);
    if (source == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate source board");
        return;
    }

    hpp_sudoku* sudoku = hpp_sudoku_create_from_board(source);
    CU_ASSERT_PTR_NOT_NULL(sudoku);
    if (sudoku != NULL)
    {
        hpp_test_assert_board_cells(sudoku->board, cells);

        source->cells[1] = 2;
        CU_ASSERT_EQUAL(sudoku->board->cells[1], BOARD_CELL_EMPTY);
        CU_ASSERT_TRUE(hpp_sudoku_is_valid(sudoku));
    }

    hpp_sudoku_destroy(&sudoku);
    destroy_board(&source);
}

// Verify that sudoku state creation rejects invalid or NULL board input
static void test_sudoku_create_from_board_rejects_invalid_input(void)
{
    // clang-format off
    const hpp_cell invalid_cells[] = {
            1, 1, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
    };
    // clang-format on

    hpp_board* source = hpp_test_create_board_from_cells(invalid_cells, 4);
    if (source == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate source board");
        return;
    }

    hpp_sudoku* sudoku = hpp_sudoku_create_from_board(source);
    CU_ASSERT_PTR_NULL(sudoku);

    destroy_board(&source);
}

// Verify that setting/clearing cells updates constraint tracking correctly
static void test_sudoku_set_and_clear_cell_updates_constraints(void)
{
    hpp_sudoku* sudoku = hpp_sudoku_create(4, 2);
    if (sudoku == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate sudoku");
        return;
    }

    CU_ASSERT_TRUE(hpp_sudoku_set_cell(sudoku, 0, 0, 1));
    CU_ASSERT_EQUAL(sudoku->board->cells[0], 1);
    CU_ASSERT_TRUE(hpp_validation_row_has_value(sudoku->constraints, 0, 1));
    CU_ASSERT_TRUE(hpp_validation_col_has_value(sudoku->constraints, 0, 1));
    CU_ASSERT_TRUE(hpp_validation_box_has_value(sudoku->constraints, 0, 0, 1));

    hpp_sudoku_clear_cell(sudoku, 0, 0);
    CU_ASSERT_EQUAL(sudoku->board->cells[0], BOARD_CELL_EMPTY);
    CU_ASSERT_FALSE(hpp_validation_row_has_value(sudoku->constraints, 0, 1));
    CU_ASSERT_FALSE(hpp_validation_col_has_value(sudoku->constraints, 0, 1));
    CU_ASSERT_FALSE(hpp_validation_box_has_value(sudoku->constraints, 0, 0, 1));

    hpp_sudoku_destroy(&sudoku);
}

static void test_sudoku_set_cell_rejects_conflict_and_range_errors(void)
{
    hpp_sudoku* sudoku = hpp_sudoku_create(4, 2);
    if (sudoku == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate sudoku");
        return;
    }

    CU_ASSERT_TRUE(hpp_sudoku_set_cell(sudoku, 0, 0, 1));
    CU_ASSERT_FALSE(hpp_sudoku_set_cell(sudoku, 0, 1, 1));
    CU_ASSERT_FALSE(hpp_sudoku_set_cell(sudoku, 4, 0, 2));
    CU_ASSERT_FALSE(hpp_sudoku_set_cell(sudoku, 0, 4, 2));
    CU_ASSERT_FALSE(hpp_sudoku_set_cell(sudoku, 0, 1, 5));

    CU_ASSERT_FALSE(hpp_sudoku_can_place_value(sudoku, 0, 2, 1));
    CU_ASSERT_TRUE(hpp_sudoku_can_place_value(sudoku, 1, 1, 2));
    CU_ASSERT_FALSE(hpp_sudoku_can_place_value(sudoku, 1, 1, BOARD_CELL_EMPTY));

    hpp_sudoku_destroy(&sudoku);
}

static void test_sudoku_clone_is_independent(void)
{
    hpp_sudoku* sudoku = hpp_sudoku_create(4, 2);
    if (sudoku == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate sudoku");
        return;
    }

    CU_ASSERT_TRUE(hpp_sudoku_set_cell(sudoku, 0, 0, 1));
    CU_ASSERT_TRUE(hpp_sudoku_set_cell(sudoku, 1, 1, 2));

    hpp_sudoku* clone = hpp_sudoku_clone(sudoku);
    CU_ASSERT_PTR_NOT_NULL(clone);
    if (clone != NULL)
    {
        CU_ASSERT_TRUE(hpp_sudoku_set_cell(clone, 0, 1, 3));
        CU_ASSERT_EQUAL(clone->board->cells[1], 3);
        CU_ASSERT_EQUAL(sudoku->board->cells[1], BOARD_CELL_EMPTY);
    }

    hpp_sudoku_destroy(&clone);
    hpp_sudoku_destroy(&sudoku);
}

static void test_sudoku_is_valid_detects_constraint_drift(void)
{
    hpp_sudoku* sudoku = hpp_sudoku_create(4, 2);
    if (sudoku == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate sudoku");
        return;
    }

    CU_ASSERT_TRUE(hpp_sudoku_set_cell(sudoku, 0, 0, 1));
    CU_ASSERT_TRUE(hpp_sudoku_is_valid(sudoku));

    hpp_validation_row_remove_value(sudoku->constraints, 0, 1);
    CU_ASSERT_FALSE(hpp_sudoku_is_valid(sudoku));

    hpp_sudoku_destroy(&sudoku);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    hpp_test_disable_logging();

    CU_pSuite suite = CU_add_suite("Sudoku", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite, "sudoku create and destroy", test_sudoku_create_and_destroy) == NULL ||
        CU_add_test(suite,
                    "sudoku create from board clones state",
                    test_sudoku_create_from_board_clones_state) == NULL ||
        CU_add_test(suite,
                    "sudoku create from board rejects invalid input",
                    test_sudoku_create_from_board_rejects_invalid_input) == NULL ||
        CU_add_test(suite,
                    "sudoku set and clear updates constraints",
                    test_sudoku_set_and_clear_cell_updates_constraints) == NULL ||
        CU_add_test(suite,
                    "sudoku set cell rejects conflict and range errors",
                    test_sudoku_set_cell_rejects_conflict_and_range_errors) == NULL ||
        CU_add_test(suite, "sudoku clone is independent", test_sudoku_clone_is_independent) ==
            NULL ||
        CU_add_test(suite,
                    "sudoku is valid detects constraint drift",
                    test_sudoku_is_valid_detects_constraint_drift) == NULL)
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
