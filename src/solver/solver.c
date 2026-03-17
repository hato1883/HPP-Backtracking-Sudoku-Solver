#include "solver/solver.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct SolverState
{
    uint64_t iterations;
    uint64_t max_iterations;
    bool     iteration_limit_reached;
} hpp_solver_state;

static size_t hpp_board_cell_index(const hpp_board* board, size_t row, size_t col)
{
    return (row * board->side_length) + col;
}

static bool hpp_board_row_has_duplicate(const hpp_board* board, size_t row, size_t col)
{
    const hpp_cell value = board->cells[hpp_board_cell_index(board, row, col)];

    if (value == BOARD_CELL_EMPTY)
    {
        return false;
    }

    for (size_t current_col = 0; current_col < board->side_length; ++current_col)
    {
        if (current_col == col)
        {
            continue;
        }

        if (board->cells[hpp_board_cell_index(board, row, current_col)] == value)
        {
            return true;
        }
    }

    return false;
}

static bool hpp_board_column_has_duplicate(const hpp_board* board, size_t row, size_t col)
{
    const hpp_cell value = board->cells[hpp_board_cell_index(board, row, col)];

    if (value == BOARD_CELL_EMPTY)
    {
        return false;
    }

    for (size_t current_row = 0; current_row < board->side_length; ++current_row)
    {
        if (current_row == row)
        {
            continue;
        }

        if (board->cells[hpp_board_cell_index(board, current_row, col)] == value)
        {
            return true;
        }
    }

    return false;
}

static bool hpp_board_box_has_duplicate(const hpp_board* board, size_t row, size_t col)
{
    const hpp_cell value = board->cells[hpp_board_cell_index(board, row, col)];

    if (value == BOARD_CELL_EMPTY)
    {
        return false;
    }

    const size_t box_row_start = (row / board->block_length) * board->block_length;
    const size_t box_col_start = (col / board->block_length) * board->block_length;

    for (size_t row_offset = 0; row_offset < board->block_length; ++row_offset)
    {
        for (size_t col_offset = 0; col_offset < board->block_length; ++col_offset)
        {
            const size_t current_row = box_row_start + row_offset;
            const size_t current_col = box_col_start + col_offset;

            if (current_row == row && current_col == col)
            {
                continue;
            }

            if (board->cells[hpp_board_cell_index(board, current_row, current_col)] == value)
            {
                return true;
            }
        }
    }

    return false;
}

static bool hpp_board_validate_guess(const hpp_board* board, size_t row, size_t col)
{
    const hpp_cell value = board->cells[hpp_board_cell_index(board, row, col)];

    if (value == BOARD_CELL_EMPTY)
    {
        return true;
    }

    if ((size_t)value > board->side_length)
    {
        return false;
    }

    return !hpp_board_row_has_duplicate(board, row, col) &&
           !hpp_board_column_has_duplicate(board, row, col) &&
           !hpp_board_box_has_duplicate(board, row, col);
}

static bool hpp_board_validate_initial_state(const hpp_board* board)
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

    for (size_t row = 0; row < board->side_length; ++row)
    {
        for (size_t col = 0; col < board->side_length; ++col)
        {
            if (!hpp_board_validate_guess(board, row, col))
            {
                return false;
            }
        }
    }

    return true;
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

        board->cells[current_index] = (hpp_cell)value;
        state->iterations++;

        if (hpp_board_validate_guess(board, row, col) &&
            hpp_backtracking_solve(board, unassigned_indices, remaining_unassigned - 1, state))
        {
            return true;
        }

        if (state->iteration_limit_reached)
        {
            break;
        }
    }

    board->cells[current_index] = BOARD_CELL_EMPTY;
    return false;
}

hpp_solver_status solve(hpp_board* board, const hpp_solver_config* config)
{
    if (!hpp_board_validate_initial_state(board))
    {
        return SOLVER_UNSOLVED;
    }

    size_t* unassigned_indices = malloc(board->cell_count * sizeof(size_t));
    if (unassigned_indices == NULL)
    {
        return SOLVER_ERROR;
    }

    const size_t     unassigned_count = hpp_collect_unassigned_indices(board, unassigned_indices);
    hpp_solver_state state            = {
                   .iterations              = 0,
                   .max_iterations          = (config != NULL) ? config->max_iterations : 0,
                   .iteration_limit_reached = false,
    };

    const bool solved = hpp_backtracking_solve(board, unassigned_indices, unassigned_count, &state);

    free(unassigned_indices);

    return solved ? SOLVER_SUCCESS : SOLVER_UNSOLVED;
}