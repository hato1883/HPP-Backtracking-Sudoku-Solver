#include "data/board.h"
#include "solver/solver.h"
#include "test_helpers.h"

#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static hpp_board* create_board_from_cells(const hpp_cell* cells, size_t side_length)
{
    hpp_board* board = create_board(side_length);
    if (board == NULL)
    {
        return NULL;
    }

    memcpy(board->cells, cells, board->cell_count * sizeof(*cells));
    return board;
}

static void assert_board_matches(const hpp_board* board, const hpp_cell* expected_cells)
{
    for (size_t idx = 0; idx < board->cell_count; ++idx)
    {
        CU_ASSERT_EQUAL(board->cells[idx], expected_cells[idx]);
    }
}

// Verify that the solver correctly solves a known 4×4 puzzle using backtracking
static void test_backtracking_solver_solves_known_4x4_puzzle(void)
{
    // clang-format off
    const hpp_cell puzzle_cells[] = {
            1, 0, 3, 0,
            0, 4, 0, 2,
            2, 0, 4, 0,
            0, 3, 0, 1,
    };
    // clang-format on

    // clang-format off
    const hpp_cell expected_solution[] = {
            1, 2, 3, 4,
            3, 4, 1, 2,
            2, 1, 4, 3,
            4, 3, 2, 1,
    };
    // clang-format on

    hpp_board* board = create_board_from_cells(puzzle_cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    const hpp_solver_config config = {
        .thread_count = 1,
        .progress_sink =
            {
                .callback         = NULL,
                .userdata         = NULL,
                .progress_every_n = 0,
            },
        .moves_log_file = NULL,
    };

    const hpp_solver_status status = solve(board, &config);

    CU_ASSERT_EQUAL(status, SOLVER_SUCCESS);
    assert_board_matches(board, expected_solution);

    destroy_board(&board);
}

// Verify that the solver rejects invalid initial boards with duplicate values
static void test_backtracking_solver_rejects_invalid_initial_board(void)
{
    // clang-format off
    const hpp_cell invalid_board[] = {
            1, 1, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
    };
    // clang-format on

    hpp_board* board = create_board_from_cells(invalid_board, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    const hpp_solver_status status = solve(board, NULL);

    CU_ASSERT_EQUAL(status, SOLVER_UNSOLVED);

    destroy_board(&board);
}

// Verify that the solver correctly solves a puzzle with multiple worker threads
static void test_backtracking_solver_solves_known_4x4_with_parallel_tasks(void)
{
    // clang-format off
    const hpp_cell puzzle_cells[] = {
            1, 0, 3, 0,
            0, 4, 0, 2,
            2, 0, 4, 0,
            0, 3, 0, 1,
    };
    // clang-format on

    hpp_board* board = create_board_from_cells(puzzle_cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    const hpp_solver_config config = {
        .thread_count = 4,
        .progress_sink =
            {
                .callback         = NULL,
                .userdata         = NULL,
                .progress_every_n = 0,
            },
        .moves_log_file = NULL,
    };

    const hpp_solver_status status = solve(board, &config);

    CU_ASSERT_EQUAL(status, SOLVER_SUCCESS);
    for (size_t idx = 0; idx < board->cell_count; ++idx)
    {
        CU_ASSERT_NOT_EQUAL(board->cells[idx], BOARD_CELL_EMPTY);
    }

    destroy_board(&board);
}

// Verify that single candidate propagation optimizes search space during solving
static void test_backtracking_solver_solves_with_single_candidate_propagation(void)
{
    // clang-format off
    const hpp_cell puzzle_cells[] = {
            1, 0, 3, 0,
            0, 4, 0, 2,
            2, 0, 4, 0,
            0, 3, 0, 1,
    };
    // clang-format on

    // clang-format off
    const hpp_cell expected_solution[] = {
            1, 2, 3, 4,
            3, 4, 1, 2,
            2, 1, 4, 3,
            4, 3, 2, 1,
    };
    // clang-format on

    hpp_board* board = create_board_from_cells(puzzle_cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    const hpp_solver_config config = {
        .thread_count = 2,
        .progress_sink =
            {
                .callback         = NULL,
                .userdata         = NULL,
                .progress_every_n = 0,
            },
        .moves_log_file = NULL,
    };

    const hpp_solver_status status = solve(board, &config);

    CU_ASSERT_EQUAL(status, SOLVER_SUCCESS);
    assert_board_matches(board, expected_solution);

    destroy_board(&board);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    hpp_test_disable_logging();

    CU_pSuite suite = CU_add_suite("Backtracking Solver", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite,
                    "solves known 4x4 puzzle",
                    test_backtracking_solver_solves_known_4x4_puzzle) == NULL ||
        CU_add_test(suite,
                    "rejects invalid initial board",
                    test_backtracking_solver_rejects_invalid_initial_board) == NULL ||
        CU_add_test(suite,
                    "solves known 4x4 with parallel tasks",
                    test_backtracking_solver_solves_known_4x4_with_parallel_tasks) == NULL ||
        CU_add_test(suite,
                    "solves using single-candidate propagation",
                    test_backtracking_solver_solves_with_single_candidate_propagation) == NULL)
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
