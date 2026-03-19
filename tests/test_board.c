#include "data/board.h"
#include "test_helpers.h"

#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <stddef.h>

static void test_create_board_initializes_shape_and_cells(void)
{
    hpp_board* board = create_board(9);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    CU_ASSERT_EQUAL(board->side_length, 9);
    CU_ASSERT_EQUAL(board->block_length, 3);
    CU_ASSERT_EQUAL(board->cell_count, 81);
    CU_ASSERT_PTR_NOT_NULL(board->cells);

    for (size_t idx = 0; idx < board->cell_count; ++idx)
    {
        CU_ASSERT_EQUAL(board->cells[idx], BOARD_CELL_EMPTY);
    }

    destroy_board(&board);
}

// Verify that cloned boards are independent copies (no shared memory)
static void test_board_clone_is_independent(void)
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

    hpp_board* clone = hpp_board_clone(source);
    CU_ASSERT_PTR_NOT_NULL(clone);
    if (clone != NULL)
    {
        CU_ASSERT_PTR_NOT_EQUAL(clone->cells, source->cells);
        hpp_test_assert_board_cells(clone, source->cells);

        clone->cells[1] = 2;
        CU_ASSERT_EQUAL(source->cells[1], BOARD_CELL_EMPTY);
    }

    destroy_board(&clone);
    destroy_board(&source);
}

// Verify that board copy operation correctly overwrites destination without freeing
static void test_board_copy_overwrites_destination(void)
{
    // clang-format off
    const hpp_cell source_cells[] = {
            1, 2, 3, 4,
            3, 4, 1, 2,
            2, 1, 4, 3,
            4, 3, 2, 1,
    };
    // clang-format on

    hpp_board* source = hpp_test_create_board_from_cells(source_cells, 4);
    hpp_board* dest   = create_board(4);
    if (source == NULL || dest == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate boards");
        destroy_board(&source);
        destroy_board(&dest);
        return;
    }

    for (size_t idx = 0; idx < dest->cell_count; ++idx)
    {
        dest->cells[idx] = (hpp_cell)((idx % dest->side_length) + 1U);
    }

    CU_ASSERT_TRUE(hpp_board_copy(dest, source));
    hpp_test_assert_board_cells(dest, source_cells);

    destroy_board(&dest);
    destroy_board(&source);
}

// Verify that printing a board doesn't modify cell values
static void test_print_board_does_not_mutate_cells(void)
{
    // clang-format off
    const hpp_cell cells[] = {
            1, 0, 3, 0,
            0, 4, 0, 2,
            2, 0, 4, 0,
            0, 3, 0, 1,
    };
    // clang-format on

    hpp_board* board = hpp_test_create_board_from_cells(cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    print_board(board);
    hpp_test_assert_board_cells(board, cells);

    destroy_board(&board);
}

static void test_destroy_board_sets_pointer_to_null(void)
{
    hpp_board* board = create_board(4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    destroy_board(&board);
    CU_ASSERT_PTR_NULL(board);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    hpp_test_disable_logging();

    CU_pSuite suite = CU_add_suite("Board", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite,
                    "create board initializes shape and cells",
                    test_create_board_initializes_shape_and_cells) == NULL ||
        CU_add_test(suite, "board clone is independent", test_board_clone_is_independent) == NULL ||
        CU_add_test(suite,
                    "board copy overwrites destination",
                    test_board_copy_overwrites_destination) == NULL ||
        CU_add_test(suite,
                    "print board does not mutate cells",
                    test_print_board_does_not_mutate_cells) == NULL ||
        CU_add_test(suite,
                    "destroy board sets pointer to null",
                    test_destroy_board_sets_pointer_to_null) == NULL)
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
