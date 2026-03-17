#include "data/board.h"
#include "solver/cactus_stack.h"

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

static void test_cactus_push_root_sets_top_and_depth(void)
{
    hpp_board* board = create_board(4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    hpp_cactus_stack stack = {0};
    hpp_cactus_stack_init(&stack);

    CU_ASSERT_EQUAL(hpp_cactus_stack_push_root_from_board(&stack, board), HPP_CANDIDATE_INIT_OK);
    CU_ASSERT_EQUAL(hpp_cactus_stack_depth(&stack), 1);

    hpp_candidate_state* root = hpp_cactus_stack_top_state(&stack);
    CU_ASSERT_PTR_NOT_NULL(root);
    if (root != NULL)
    {
        CU_ASSERT_EQUAL(root->remaining_unassigned, 16);
    }

    hpp_cactus_stack_destroy(&stack);
    destroy_board(&board);
}

static void test_cactus_clone_commit_and_pop(void)
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

    hpp_cactus_stack stack = {0};
    hpp_cactus_stack_init(&stack);

    if (hpp_cactus_stack_push_root_from_board(&stack, board) != HPP_CANDIDATE_INIT_OK)
    {
        CU_FAIL_FATAL("Failed to initialize cactus root");
        hpp_cactus_stack_destroy(&stack);
        destroy_board(&board);
        return;
    }

    CU_ASSERT_TRUE(hpp_cactus_stack_push_clone(&stack));
    CU_ASSERT_TRUE(hpp_candidate_state_assign(hpp_cactus_stack_top_state(&stack), 3, 4));
    CU_ASSERT_TRUE(hpp_cactus_stack_pop(&stack));
    CU_ASSERT_EQUAL(hpp_cactus_stack_top_state(&stack)->board->cells[3], BOARD_CELL_EMPTY);

    CU_ASSERT_TRUE(hpp_cactus_stack_push_clone(&stack));
    CU_ASSERT_TRUE(hpp_candidate_state_assign(hpp_cactus_stack_top_state(&stack), 3, 4));
    CU_ASSERT_TRUE(hpp_cactus_stack_commit_top_to_parent(&stack));
    CU_ASSERT_TRUE(hpp_cactus_stack_pop(&stack));
    CU_ASSERT_EQUAL(hpp_cactus_stack_top_state(&stack)->board->cells[3], 4);

    hpp_cactus_stack_destroy(&stack);
    destroy_board(&board);
}

static void test_cactus_operations_fail_without_root(void)
{
    hpp_cactus_stack stack = {0};
    hpp_cactus_stack_init(&stack);

    CU_ASSERT_FALSE(hpp_cactus_stack_push_clone(&stack));
    CU_ASSERT_FALSE(hpp_cactus_stack_commit_top_to_parent(&stack));
    CU_ASSERT_FALSE(hpp_cactus_stack_pop(&stack));
    CU_ASSERT_PTR_NULL(hpp_cactus_stack_top_state(&stack));
    CU_ASSERT_EQUAL(hpp_cactus_stack_depth(&stack), 0);
}

static void test_cactus_rejects_invalid_root_board(void)
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

    hpp_cactus_stack stack = {0};
    hpp_cactus_stack_init(&stack);

    CU_ASSERT_EQUAL(hpp_cactus_stack_push_root_from_board(&stack, board),
                    HPP_CANDIDATE_INIT_INVALID_BOARD);
    CU_ASSERT_EQUAL(hpp_cactus_stack_depth(&stack), 0);
    CU_ASSERT_PTR_NULL(hpp_cactus_stack_top_state(&stack));

    hpp_cactus_stack_destroy(&stack);
    destroy_board(&board);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    CU_pSuite suite = CU_add_suite("Cactus Stack", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite,
                    "push root sets top and depth",
                    test_cactus_push_root_sets_top_and_depth) == NULL ||
        CU_add_test(suite, "clone commit and pop", test_cactus_clone_commit_and_pop) == NULL ||
        CU_add_test(suite,
                    "operations fail without root",
                    test_cactus_operations_fail_without_root) == NULL ||
        CU_add_test(suite, "rejects invalid root board", test_cactus_rejects_invalid_root_board) ==
            NULL)
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
