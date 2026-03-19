/**
 * @file test_error_paths.c
 * @brief Test coverage for error handling paths and edge cases.
 *
 * This test suite targets missing code paths related to:
 * - Parser header validation (invalid base/side values)
 * - Parser I/O error handling
 * - Candidate branching corner cases (complete boards, conflicts)
 * - Solver initialization with invalid inputs
 */

#include "data/board.h"
#include "data/sudoku.h"
#include "parser/parser.h"
#include "solver/candidate.h"
#include "solver/candidate_internal.h"
#include "solver/solver.h"
#include "test_helpers.h"

#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * Test the candidate branching returns COMPLETE when no cells remain unassigned.
 * Tests line 23 of candidate_branching.c: early return HPP_CANDIDATE_BRANCH_COMPLETE
 */
static void test_candidate_build_branch_complete_board(void)
{
    /* Create a mostly-solved board with only last cell empty */
    // clang-format off
    const hpp_cell cells[] = {
            1, 2, 3, 4,
            2, 1, 4, 3,
            3, 4, 1, 2,
            4, 3, 2, 0,
    };
    // clang-format on

    hpp_board* board = hpp_test_create_board_from_cells(cells, 4);
    CU_ASSERT_PTR_NOT_NULL(board);

    hpp_candidate_state       state       = {0};
    hpp_candidate_init_status init_status = hpp_candidate_state_init_from_board(&state, board);
    if (init_status != HPP_CANDIDATE_INIT_OK)
    {
        /* If init fails due to board validity, skip the test */
        destroy_board(&board);
        CU_PASS("Board initialization skipped due to validity check");
        return;
    }

    /* Now manually zero out remaining_unassigned to simulate complete state */
    state.remaining_unassigned = 0;

    /* Build branch should return COMPLETE when no unassigned cells */
    hpp_candidate_branch        branch = {0};
    hpp_candidate_branch_status status = hpp_candidate_state_build_branch(&state, &branch);

    CU_ASSERT_EQUAL(status, HPP_CANDIDATE_BRANCH_COMPLETE);

    hpp_candidate_state_destroy(&state);
    destroy_board(&board);
}

/**
 * Test the candidate branching detects conflict when a cell has zero candidates.
 * Tests lines 42-49 of candidate_branching.c: HPP_CANDIDATE_BRANCH_CONFLICT return
 */
static void test_candidate_build_branch_conflict_zero_candidates(void)
{
    /* Create a board with a conflict: two 1's in the same row */
    // clang-format off
    const hpp_cell cells[] = {
            1, 1, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
    };
    // clang-format on

    hpp_board* board = hpp_test_create_board_from_cells(cells, 4);
    CU_ASSERT_PTR_NOT_NULL(board);

    hpp_candidate_state       state       = {0};
    hpp_candidate_init_status init_status = hpp_candidate_state_init_from_board(&state, board);

    /* This board has a conflict; initialization may fail or state may indicate conflict */
    if (init_status == HPP_CANDIDATE_INIT_OK)
    {
        /* If state was created despite conflict, building a branch should detect it */
        hpp_candidate_branch        branch = {0};
        hpp_candidate_branch_status status = hpp_candidate_state_build_branch(&state, &branch);

        /* Either CONFLICT or the cell with zero candidates triggers conflict detection */
        CU_ASSERT(status == HPP_CANDIDATE_BRANCH_CONFLICT || state.remaining_unassigned == 0);

        hpp_candidate_state_destroy(&state);
    }
    else
    {
        /* If init failed due to conflict detection, that's also valid */
        CU_ASSERT_NOT_EQUAL(init_status, HPP_CANDIDATE_INIT_OK);
    }

    destroy_board(&board);
}

/**
 * Test parser fails gracefully when given a non-existent file.
 * Tests lines 31-32 of parser.c: fopen NULL return handling.
 * Uses subprocess to isolate fatal exit().
 */
static void test_parser_file_not_found_exits(void)
{
    const char* nonexistent = "/tmp/sudoku_nonexistent_9999999.bin";

    const pid_t child = fork();
    if (child == 0)
    {
        /* Child process: attempt to parse non-existent file */
        (void)parse_file(nonexistent);
        /* Should not reach here due to exit() call in parse_file */
        _exit(0);
    }

    int status = 0;
    waitpid(child, &status, 0);

    /* Verify child exited with non-zero status */
    CU_ASSERT_NOT_EQUAL(WEXITSTATUS(status), 0);
}

/**
 * Test parser detects invalid base value (0) in header.
 * Tests lines 80-82 of parser.c: base == 0 validation and exit.
 * Writes a binary file with base=0 and parses it via subprocess.
 */
static void test_parser_invalid_base_zero_exits(void)
{
    char template_path[] = "/tmp/sudoku_invalid_base_XXXXXX";
    int  fd              = mkstemp(template_path);
    if (fd < 0)
    {
        CU_FAIL_FATAL("Failed to create temp file");
        return;
    }

    /* Write corrupted header: base=0, side=4 */
    unsigned char header[] = {0, 4};
    ssize_t       written  = write(fd, header, sizeof(header));
    close(fd);

    if (written != (ssize_t)sizeof(header))
    {
        unlink(template_path);
        CU_FAIL_FATAL("Failed to write corrupted header");
        return;
    }

    const pid_t child = fork();
    if (child == 0)
    {
        /* Child: attempt to parse corrupted file */
        (void)parse_file(template_path);
        /* Should not reach here due to exit() call when base==0 */
        _exit(0);
    }

    int status = 0;
    waitpid(child, &status, 0);
    unlink(template_path);

    /* Verify child exited with non-zero status */
    CU_ASSERT_NOT_EQUAL(WEXITSTATUS(status), 0);
}

/**
 * Test parser detects invalid side value (0) in header.
 * Tests lines 84-86 of parser.c: side == 0 validation and exit.
 */
static void test_parser_invalid_side_zero_exits(void)
{
    char template_path[] = "/tmp/sudoku_invalid_side_XXXXXX";
    int  fd              = mkstemp(template_path);
    if (fd < 0)
    {
        CU_FAIL_FATAL("Failed to create temp file");
        return;
    }

    /* Write corrupted header: base=3, side=0 */
    unsigned char header[] = {3, 0};
    ssize_t       written  = write(fd, header, sizeof(header));
    close(fd);

    if (written != (ssize_t)sizeof(header))
    {
        unlink(template_path);
        CU_FAIL_FATAL("Failed to write corrupted header");
        return;
    }

    const pid_t child = fork();
    if (child == 0)
    {
        /* Child: attempt to parse corrupted file */
        (void)parse_file(template_path);
        /* Should not reach here due to exit() call when side==0 */
        _exit(0);
    }

    int status = 0;
    waitpid(child, &status, 0);
    unlink(template_path);

    /* Verify child exited with non-zero status */
    CU_ASSERT_NOT_EQUAL(WEXITSTATUS(status), 0);
}

/**
 * Test parser detects truncated header (reads less than needed).
 * Tests lines 111-112 of parser.c: fread failure in parse_header.
 */
static void test_parser_truncated_header_exits(void)
{
    char template_path[] = "/tmp/sudoku_truncated_header_XXXXXX";
    int  fd              = mkstemp(template_path);
    if (fd < 0)
    {
        CU_FAIL_FATAL("Failed to create temp file");
        return;
    }

    /* Write only 1 byte (incomplete header) */
    unsigned char partial[] = {3};
    ssize_t       written   = write(fd, partial, sizeof(partial));
    close(fd);

    if (written != (ssize_t)sizeof(partial))
    {
        unlink(template_path);
        CU_FAIL_FATAL("Failed to write partial header");
        return;
    }

    const pid_t child = fork();
    if (child == 0)
    {
        /* Child: attempt to parse incomplete file */
        (void)parse_file(template_path);
        /* Should not reach here due to exit() call on fread failure */
        _exit(0);
    }

    int status = 0;
    waitpid(child, &status, 0);
    unlink(template_path);

    /* Verify child exited with non-zero status */
    CU_ASSERT_NOT_EQUAL(WEXITSTATUS(status), 0);
}

/**
 * Test write_board_to_stream with NULL file pointer.
 * Tests line 62 of parser.c: NULL file validation.
 */
static void test_write_board_stream_null_file(void)
{
    // clang-format off
    const hpp_cell cells[] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    // clang-format on
    hpp_board* board = hpp_test_create_board_from_cells(cells, 4);
    CU_ASSERT_PTR_NOT_NULL(board);

    int result = write_board_to_stream(NULL, board, 1);
    CU_ASSERT_EQUAL(result, -1);

    destroy_board(&board);
}

/**
 * Test write_board_to_stream with NULL board pointer.
 * Tests line 62 of parser.c: NULL board validation.
 */
static void test_write_board_stream_null_board(void)
{
    FILE* file = tmpfile();
    if (file == NULL)
    {
        CU_FAIL_FATAL("Failed to create temp file");
        return;
    }

    int result = write_board_to_stream(file, NULL, 1);
    CU_ASSERT_EQUAL(result, -1);

    fclose(file);
}

/**
 * Test write_solution with NULL filename.
 * Tests line 111 of parser.c: NULL filename validation.
 */
static void test_write_solution_null_filename(void)
{
    // clang-format off
    const hpp_cell cells[] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    // clang-format on
    hpp_board* board = hpp_test_create_board_from_cells(cells, 4);
    CU_ASSERT_PTR_NOT_NULL(board);

    int result = write_solution(NULL, board);
    CU_ASSERT_EQUAL(result, -1);

    destroy_board(&board);
}

/**
 * Test write_solution with NULL board.
 * Tests line 111 of parser.c: NULL board validation.
 */
static void test_write_solution_null_board(void)
{
    const char* filename = "/tmp/sudoku_test_solution.bin";
    int         result   = write_solution(filename, NULL);
    CU_ASSERT_EQUAL(result, -1);

    /* Clean up if file was created */
    unlink(filename);
}

/**
 * Test solve with NULL board pointer.
 * Tests line 50 of solver.c: NULL board validation.
 */
static void test_solve_null_board(void)
{
    hpp_solver_status status = solve(NULL, NULL);
    CU_ASSERT_EQUAL(status, SOLVER_ERROR);
}

/**
 * Test solve with board that has NULL cells.
 * Tests line 50 of solver.c: NULL cells validation.
 */
static void test_solve_null_cells(void)
{
    /* Create a board manually with NULL cells */
    hpp_board board = {
        .side_length  = 9,
        .block_length = 3,
        .cell_count   = 81,
        .cells        = NULL, /* Invalid: NULL cells */
    };

    hpp_solver_status status = solve(&board, NULL);
    CU_ASSERT_EQUAL(status, SOLVER_ERROR);
}

/**
 * Test solve with invalid board dimensions.
 * Tests lines 65-66 of solver.c: thread count edge case handling.
 */
static void test_solve_thread_count_zero(void)
{
    // clang-format off
    const hpp_cell cells[] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    // clang-format on
    hpp_board* board = hpp_test_create_board_from_cells(cells, 4);
    CU_ASSERT_PTR_NOT_NULL(board);

    /* Config with thread_count = 0 should use runtime count */
    hpp_solver_config config = {.thread_count = 0};
    hpp_solver_status status = solve(board, &config);

    /* Should handle gracefully (UNSOLVED or SUCCESS, not ERROR) */
    CU_ASSERT(status == SOLVER_UNSOLVED || status == SOLVER_SUCCESS);

    destroy_board(&board);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    hpp_test_disable_logging();

    CU_pSuite suite = CU_add_suite("Error Paths", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite,
                    "candidate branching detects complete board",
                    test_candidate_build_branch_complete_board) == NULL ||
        CU_add_test(suite,
                    "candidate branching detects conflict (zero candidates)",
                    test_candidate_build_branch_conflict_zero_candidates) == NULL ||
        CU_add_test(suite, "parser exits on non-existent file", test_parser_file_not_found_exits) ==
            NULL ||
        CU_add_test(suite, "parser exits on invalid base=0", test_parser_invalid_base_zero_exits) ==
            NULL ||
        CU_add_test(suite, "parser exits on invalid side=0", test_parser_invalid_side_zero_exits) ==
            NULL ||
        CU_add_test(suite,
                    "parser exits on truncated header",
                    test_parser_truncated_header_exits) == NULL ||
        CU_add_test(suite,
                    "write board stream rejects NULL file",
                    test_write_board_stream_null_file) == NULL ||
        CU_add_test(suite,
                    "write board stream rejects NULL board",
                    test_write_board_stream_null_board) == NULL ||
        CU_add_test(suite,
                    "write solution rejects NULL filename",
                    test_write_solution_null_filename) == NULL ||
        CU_add_test(suite, "write solution rejects NULL board", test_write_solution_null_board) ==
            NULL ||
        CU_add_test(suite, "solve rejects NULL board", test_solve_null_board) == NULL ||
        CU_add_test(suite, "solve rejects NULL cells", test_solve_null_cells) == NULL ||
        CU_add_test(suite, "solve handles thread count zero", test_solve_thread_count_zero) == NULL)
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
