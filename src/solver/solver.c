#include "solver/solver.h"

#include "solver/cactus_stack.h"
#include "solver/candidate.h"

#include <stdbool.h>

static bool
hpp_solver_search(hpp_cactus_stack* stack, hpp_candidate_budget* budget, bool* internal_error)
{
    hpp_candidate_state* state = hpp_cactus_stack_top_state(stack);
    if (state == NULL)
    {
        *internal_error = true;
        return false;
    }

    if (!hpp_candidate_state_propagate_singles(state, budget))
    {
        return false;
    }

    if (budget->iteration_limit_reached)
    {
        return false;
    }

    if (hpp_candidate_state_is_complete(state))
    {
        return true;
    }

    hpp_candidate_branch              branch = {0};
    const hpp_candidate_branch_status branch_status =
        hpp_candidate_state_build_branch(state, &branch);

    if (branch_status == HPP_CANDIDATE_BRANCH_COMPLETE)
    {
        return true;
    }

    if (branch_status == HPP_CANDIDATE_BRANCH_CONFLICT)
    {
        return false;
    }

    for (size_t idx = 0; idx < branch.value_count; ++idx)
    {
        if (!hpp_cactus_stack_push_clone(stack))
        {
            *internal_error = true;
            return false;
        }

        hpp_candidate_state* child_state = hpp_cactus_stack_top_state(stack);
        if (child_state == NULL)
        {
            *internal_error = true;
            return false;
        }

        const size_t guess = branch.values[idx];
        if (hpp_candidate_state_assign(child_state, branch.cell_index, guess, budget) &&
            hpp_solver_search(stack, budget, internal_error))
        {
            if (!hpp_cactus_stack_commit_top_to_parent(stack) || !hpp_cactus_stack_pop(stack))
            {
                *internal_error = true;
                return false;
            }

            return true;
        }

        if (!hpp_cactus_stack_pop(stack))
        {
            *internal_error = true;
            return false;
        }

        if (*internal_error || budget->iteration_limit_reached)
        {
            return false;
        }
    }

    return false;
}

hpp_solver_status solve(hpp_board* board, const hpp_solver_config* config)
{
    if (board == NULL || board->cells == NULL)
    {
        return SOLVER_ERROR;
    }

    hpp_cactus_stack stack = {0};
    hpp_cactus_stack_init(&stack);

    const hpp_candidate_init_status init_status =
        hpp_cactus_stack_push_root_from_board(&stack, board);

    if (init_status == HPP_CANDIDATE_INIT_INVALID_BOARD)
    {
        hpp_cactus_stack_destroy(&stack);
        return SOLVER_UNSOLVED;
    }

    if (init_status != HPP_CANDIDATE_INIT_OK)
    {
        hpp_cactus_stack_destroy(&stack);
        return SOLVER_ERROR;
    }

    hpp_candidate_budget budget = {
        .iterations              = 0,
        .max_iterations          = (config != NULL) ? config->max_iterations : 0,
        .iteration_limit_reached = false,
    };

    bool internal_error = false;

    const bool solved = hpp_solver_search(&stack, &budget, &internal_error);
    if (solved)
    {
        const hpp_candidate_state* root_state = hpp_cactus_stack_top_state_const(&stack);
        if (root_state == NULL)
        {
            internal_error = true;
        }
        else
        {
            hpp_board_copy(board, root_state->board);
        }
    }

    hpp_cactus_stack_destroy(&stack);

    if (internal_error)
    {
        return SOLVER_ERROR;
    }

    return solved ? SOLVER_SUCCESS : SOLVER_UNSOLVED;
}
