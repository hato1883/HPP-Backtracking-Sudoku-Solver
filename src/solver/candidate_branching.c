/**
 * @file solver/candidate_branching.c
 * @brief Branch-selection helpers for candidate-guided backtracking.
 */

#include "solver/candidate_internal.h"

hpp_candidate_branch_status hpp_candidate_state_build_branch(hpp_candidate_state*  state,
                                                             hpp_candidate_branch* branch)
{
    if (state->remaining_unassigned == 0)
    {
        return HPP_CANDIDATE_BRANCH_COMPLETE;
    }

    size_t best_cell  = SIZE_MAX;
    size_t best_count = SIZE_MAX;

    for (size_t cell_index = 0; cell_index < state->board->cell_count; ++cell_index)
    {
        if (state->board->cells[cell_index] != BOARD_CELL_EMPTY)
        {
            continue;
        }

        const size_t candidate_count = state->candidate_counts[cell_index];
        if (candidate_count == 0)
        {
            return HPP_CANDIDATE_BRANCH_CONFLICT;
        }

        if (candidate_count < best_count)
        {
            best_count = candidate_count;
            best_cell  = cell_index;

            if (best_count == 2)
            {
                break;
            }
        }
    }

    if (best_cell == SIZE_MAX)
    {
        return HPP_CANDIDATE_BRANCH_CONFLICT;
    }

    branch->cell_index  = best_cell;
    branch->value_count = 0;

    const hpp_bitvector_word* best_candidates = hpp_candidate_cell_at_const(state, best_cell);
    for (size_t value = 1; value <= state->board->side_length; ++value)
    {
        if (!hpp_candidate_bit_test(best_candidates, value))
        {
            continue;
        }

        branch->values[branch->value_count++] = value;
    }

    return (branch->value_count == 0) ? HPP_CANDIDATE_BRANCH_CONFLICT : HPP_CANDIDATE_BRANCH_READY;
}

bool hpp_candidate_state_is_complete(const hpp_candidate_state* state)
{
    return state->remaining_unassigned == 0;
}
