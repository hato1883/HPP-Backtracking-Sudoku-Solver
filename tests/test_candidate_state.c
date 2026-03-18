#include "data/board.h"
#include "solver/candidate.h"

#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
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

static void test_candidate_init_rejects_invalid_board(void)
{
    const hpp_cell invalid_cells[] = {
        1,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    };

    hpp_board* board = create_board_from_cells(invalid_cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    hpp_candidate_state             state  = {0};
    const hpp_candidate_init_status status = hpp_candidate_state_init_from_board(&state, board);

    CU_ASSERT_EQUAL(status, HPP_CANDIDATE_INIT_INVALID_BOARD);
    CU_ASSERT_PTR_NULL(state.board);

    hpp_candidate_state_destroy(&state);
    destroy_board(&board);
}

static void test_candidate_assign_rejects_row_conflict(void)
{
    hpp_board* board = create_board(4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    hpp_candidate_state state = {0};
    if (hpp_candidate_state_init_from_board(&state, board) != HPP_CANDIDATE_INIT_OK)
    {
        CU_FAIL_FATAL("Failed to initialize candidate state");
        destroy_board(&board);
        return;
    }

    CU_ASSERT_TRUE(hpp_candidate_state_assign(&state, 0, 1));
    CU_ASSERT_FALSE(hpp_candidate_state_assign(&state, 1, 1));
    CU_ASSERT_EQUAL(state.board->cells[1], BOARD_CELL_EMPTY);

    hpp_candidate_state_destroy(&state);
    destroy_board(&board);
}

static void test_candidate_clone_is_independent(void)
{
    const hpp_cell cells[] = {
        1,
        2,
        3,
        0,
        3,
        4,
        1,
        2,
        2,
        1,
        4,
        3,
        4,
        3,
        2,
        1,
    };

    hpp_board* board = create_board_from_cells(cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    hpp_candidate_state source = {0};
    if (hpp_candidate_state_init_from_board(&source, board) != HPP_CANDIDATE_INIT_OK)
    {
        CU_FAIL_FATAL("Failed to initialize source state");
        destroy_board(&board);
        return;
    }

    hpp_candidate_state clone = {0};
    CU_ASSERT_TRUE(hpp_candidate_state_clone(&source, &clone));

    CU_ASSERT_TRUE(hpp_candidate_state_assign(&clone, 3, 4));
    CU_ASSERT_EQUAL(clone.board->cells[3], 4);
    CU_ASSERT_EQUAL(source.board->cells[3], BOARD_CELL_EMPTY);

    hpp_candidate_state_destroy(&clone);
    hpp_candidate_state_destroy(&source);
    destroy_board(&board);
}

static void test_candidate_propagates_singles_to_completion(void)
{
    const hpp_cell cells[] = {
        1,
        2,
        3,
        0,
        3,
        4,
        1,
        2,
        2,
        1,
        4,
        3,
        4,
        3,
        2,
        1,
    };

    const hpp_cell expected[] = {
        1,
        2,
        3,
        4,
        3,
        4,
        1,
        2,
        2,
        1,
        4,
        3,
        4,
        3,
        2,
        1,
    };

    hpp_board* board = create_board_from_cells(cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    hpp_candidate_state state = {0};
    if (hpp_candidate_state_init_from_board(&state, board) != HPP_CANDIDATE_INIT_OK)
    {
        CU_FAIL_FATAL("Failed to initialize candidate state");
        destroy_board(&board);
        return;
    }

    CU_ASSERT_TRUE(hpp_candidate_state_propagate_singles(&state));
    CU_ASSERT_TRUE(hpp_candidate_state_is_complete(&state));
    assert_board_matches(state.board, expected);

    hpp_candidate_state_destroy(&state);
    destroy_board(&board);
}

static void test_candidate_propagates_hidden_single(void)
{
    const hpp_cell cells[] = {
        1,
        0,
        0,
        4,
        0,
        4,
        0,
        0,
        0,
        0,
        4,
        0,
        4,
        0,
        0,
        1,
    };

    hpp_board* board = create_board_from_cells(cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    hpp_candidate_state state = {0};
    if (hpp_candidate_state_init_from_board(&state, board) != HPP_CANDIDATE_INIT_OK)
    {
        CU_FAIL_FATAL("Failed to initialize candidate state");
        destroy_board(&board);
        return;
    }

    CU_ASSERT_TRUE(hpp_candidate_state_propagate_singles(&state));
    CU_ASSERT_EQUAL(state.board->cells[(1U * 4U) + 2U], 1);

    hpp_candidate_state_destroy(&state);
    destroy_board(&board);
}

static void test_candidate_build_branch_detects_conflict(void)
{
    const hpp_cell cells[] = {
        1,
        2,
        3,
        0,
        0,
        0,
        0,
        4,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    };

    hpp_board* board = create_board_from_cells(cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    hpp_candidate_state state = {0};
    if (hpp_candidate_state_init_from_board(&state, board) != HPP_CANDIDATE_INIT_OK)
    {
        CU_FAIL_FATAL("Failed to initialize candidate state");
        destroy_board(&board);
        return;
    }

    hpp_candidate_branch branch = {0};
    CU_ASSERT_EQUAL(hpp_candidate_state_build_branch(&state, &branch),
                    HPP_CANDIDATE_BRANCH_CONFLICT);

    hpp_candidate_state_destroy(&state);
    destroy_board(&board);
}

static void test_candidate_propagation_detects_zero_candidate_cell(void)
{
    const hpp_cell cells[] = {
        0,
        2,
        3,
        4,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    };

    hpp_board* board = create_board_from_cells(cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    hpp_candidate_state state = {0};
    if (hpp_candidate_state_init_from_board(&state, board) != HPP_CANDIDATE_INIT_OK)
    {
        CU_FAIL_FATAL("Failed to initialize candidate state");
        destroy_board(&board);
        return;
    }

    CU_ASSERT_FALSE(hpp_candidate_state_propagate_singles(&state));

    hpp_candidate_state_destroy(&state);
    destroy_board(&board);
}

static void test_candidate_propagation_detects_peer_elimination_conflict(void)
{
    const hpp_cell cells[] = {
        0,
        0,
        3,
        4,
        2,
        3,
        0,
        0,
        3,
        0,
        0,
        0,
        4,
        2,
        0,
        0,
    };

    hpp_board* board = create_board_from_cells(cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    hpp_candidate_state state = {0};
    if (hpp_candidate_state_init_from_board(&state, board) != HPP_CANDIDATE_INIT_OK)
    {
        CU_FAIL_FATAL("Failed to initialize candidate state");
        destroy_board(&board);
        return;
    }

    CU_ASSERT_FALSE(hpp_candidate_state_propagate_singles(&state));

    hpp_candidate_state_destroy(&state);
    destroy_board(&board);
}

static void test_candidate_ac3_detects_conflicting_singleton_peers(void)
{
    const hpp_cell cells[] = {
        0,
        0,
        3,
        4,
        2,
        3,
        1,
        0,
        3,
        2,
        0,
        0,
        4,
        0,
        0,
        0,
    };

    hpp_board* board = create_board_from_cells(cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    hpp_candidate_state state = {0};
    if (hpp_candidate_state_init_from_board(&state, board) != HPP_CANDIDATE_INIT_OK)
    {
        CU_FAIL_FATAL("Failed to initialize candidate state");
        destroy_board(&board);
        return;
    }

    // (0,0) and (0,1) both collapse to singleton {1} and conflict as peers.
    CU_ASSERT_FALSE(hpp_candidate_state_propagate_singles(&state));

    hpp_candidate_state_destroy(&state);
    destroy_board(&board);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    CU_pSuite suite = CU_add_suite("Candidate State", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite,
                    "init rejects invalid board",
                    test_candidate_init_rejects_invalid_board) == NULL ||
        CU_add_test(suite,
                    "assign rejects row conflict",
                    test_candidate_assign_rejects_row_conflict) == NULL ||
        CU_add_test(suite, "clone is independent", test_candidate_clone_is_independent) == NULL ||
        CU_add_test(suite,
                    "propagates singles to completion",
                    test_candidate_propagates_singles_to_completion) == NULL ||
        CU_add_test(suite, "propagates hidden singles", test_candidate_propagates_hidden_single) ==
            NULL ||
        CU_add_test(suite,
                    "build branch detects conflict",
                    test_candidate_build_branch_detects_conflict) == NULL ||
        CU_add_test(suite,
                    "propagation detects zero-candidate contradiction",
                    test_candidate_propagation_detects_zero_candidate_cell) == NULL ||
        CU_add_test(suite,
                    "propagation detects peer elimination contradiction",
                    test_candidate_propagation_detects_peer_elimination_conflict) == NULL ||
        CU_add_test(suite,
                    "AC-3 detects conflicting singleton peers",
                    test_candidate_ac3_detects_conflicting_singleton_peers) == NULL)
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
