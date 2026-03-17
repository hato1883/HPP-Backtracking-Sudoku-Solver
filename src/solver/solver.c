#include "solver/solver.h"

#include "data/validation.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct SolverState
{
    uint64_t                    iterations;
    uint64_t                    max_iterations;
    bool                        iteration_limit_reached;
    hpp_validation_constraints* constraints;
} hpp_solver_state;

static inline bool hpp_validate_guess(const hpp_validation_constraints* constraints,
                                      size_t                            row,
                                      size_t                            col,
                                      size_t                            value)
{
    if (value == BOARD_CELL_EMPTY)
    {
        return true;
    }

    if (value > constraints->side_length)
    {
        return false;
    }

    return hpp_validation_can_place_value(constraints, row, col, value);
}

static bool hpp_board_validate_initial_state(const hpp_board*            board,
                                             hpp_validation_constraints* constraints)
{
    if (board == NULL || board->cells == NULL || board->side_length == 0 ||
        board->block_length == 0 || board->cell_count != board->side_length * board->side_length)
    {
        return false;
    }

    if (board->block_length * board->block_length != board->side_length ||
        board->side_length > UINT8_MAX)
    {
        return false;
    }

    return hpp_validation_constraints_init_from_board(constraints, board);
}

static size_t hpp_collect_unassigned_indices(const hpp_board* board, size_t* unassigned_indices)
{
    size_t unassigned_count = 0;

    for (size_t index = 0; index < board->cell_count; ++index)
    {
        if (board->cells[index] == BOARD_CELL_EMPTY)
        {
            unassigned_indices[unassigned_count++] = index;
        }
    }

    return unassigned_count;
}

static bool hpp_backtracking_solve(hpp_board*        board,
                                   const size_t*     unassigned_indices,
                                   size_t            remaining_unassigned,
                                   hpp_solver_state* state)
{
    if (remaining_unassigned == 0)
    {
        return true;
    }

    if (state->iteration_limit_reached)
    {
        return false;
    }

    const size_t current_index = unassigned_indices[remaining_unassigned - 1];
    const size_t row           = current_index / board->side_length;
    const size_t col           = current_index % board->side_length;

    for (size_t value = 1; value <= board->side_length; ++value)
    {
        if (state->max_iterations != 0 && state->iterations >= state->max_iterations)
        {
            state->iteration_limit_reached = true;
            break;
        }

        // Check if value can be placed using constraint validation
        if (!hpp_validate_guess(state->constraints, row, col, value))
        {
            state->iterations++;
            continue;
        }

        // Place the value
        board->cells[current_index] = (hpp_cell)value;
        hpp_validation_row_add_value(state->constraints, row, value);
        hpp_validation_col_add_value(state->constraints, col, value);
        hpp_validation_box_add_value(state->constraints, row, col, value);
        state->iterations++;

        if (hpp_backtracking_solve(board, unassigned_indices, remaining_unassigned - 1, state))
        {
            return true;
        }

        // Remove the value and try next
        board->cells[current_index] = BOARD_CELL_EMPTY;
        hpp_validation_row_remove_value(state->constraints, row, value);
        hpp_validation_col_remove_value(state->constraints, col, value);
        hpp_validation_box_remove_value(state->constraints, row, col, value);

        if (state->iteration_limit_reached)
        {
            break;
        }
    }

    return false;
}

hpp_solver_status solve(hpp_board* board, const hpp_solver_config* config)
{
    hpp_validation_constraints* constraints =
        hpp_validation_constraints_create(board->side_length, board->block_length);
    if (constraints == NULL)
    {
        return SOLVER_ERROR;
    }

    if (!hpp_board_validate_initial_state(board, constraints))
    {
        hpp_validation_constraints_destroy(&constraints);
        return SOLVER_UNSOLVED;
    }

    size_t* unassigned_indices = malloc(board->cell_count * sizeof(size_t));
    if (unassigned_indices == NULL)
    {
        hpp_validation_constraints_destroy(&constraints);
        return SOLVER_ERROR;
    }

    const size_t     unassigned_count = hpp_collect_unassigned_indices(board, unassigned_indices);
    hpp_solver_state state            = {
                   .iterations              = 0,
                   .max_iterations          = (config != NULL) ? config->max_iterations : 0,
                   .iteration_limit_reached = false,
                   .constraints             = constraints,
    };

    const bool solved = hpp_backtracking_solve(board, unassigned_indices, unassigned_count, &state);

    free(unassigned_indices);
    hpp_validation_constraints_destroy(&constraints);

    return solved ? SOLVER_SUCCESS : SOLVER_UNSOLVED;
}