/**
 * @file data/sudoku.c
 * @brief High-level Sudoku wrapper around board and validation constraints.
 */

#include "data/sudoku.h"

#include <stdlib.h>

/* =========================================================================
 * Forward Declarations
 * ========================================================================= */

static size_t hpp_sudoku_infer_block_length(size_t side_length);

/* =========================================================================
 * Public API
 * ========================================================================= */

hpp_sudoku* hpp_sudoku_create(size_t side_length, size_t block_length)
{
    hpp_sudoku* sudoku = malloc(sizeof(hpp_sudoku));
    if (sudoku == NULL)
    {
        return NULL;
    }

    sudoku->board = create_board(side_length);
    if (sudoku->board == NULL)
    {
        free(sudoku);
        return NULL;
    }

    sudoku->constraints = hpp_validation_constraints_create(side_length, block_length);
    if (sudoku->constraints == NULL)
    {
        destroy_board(&sudoku->board);
        free(sudoku);
        return NULL;
    }

    return sudoku;
}

hpp_sudoku* hpp_sudoku_create_from_board(const hpp_board* board)
{
    if (board == NULL)
    {
        return NULL;
    }

    hpp_sudoku* sudoku = malloc(sizeof(hpp_sudoku));
    if (sudoku == NULL)
    {
        return NULL;
    }

    sudoku->board = hpp_board_clone(board);
    if (sudoku->board == NULL)
    {
        free(sudoku);
        return NULL;
    }

    const size_t block_length = hpp_sudoku_infer_block_length(board->side_length);
    if (block_length == 0)
    {
        destroy_board(&sudoku->board);
        free(sudoku);
        return NULL;
    }

    sudoku->constraints = hpp_validation_constraints_create(board->side_length, block_length);
    if (sudoku->constraints == NULL)
    {
        destroy_board(&sudoku->board);
        free(sudoku);
        return NULL;
    }

    // Initialize constraints from the copied board state.
    if (!hpp_validation_constraints_init_from_board(sudoku->constraints, sudoku->board))
    {
        hpp_validation_constraints_destroy(&sudoku->constraints);
        destroy_board(&sudoku->board);
        free(sudoku);
        return NULL;
    }

    return sudoku;
}

hpp_sudoku* hpp_sudoku_clone(const hpp_sudoku* sudoku)
{
    if (sudoku == NULL)
    {
        return NULL;
    }

    hpp_sudoku* cloned = malloc(sizeof(hpp_sudoku));
    if (cloned == NULL)
    {
        return NULL;
    }

    cloned->board = hpp_board_clone(sudoku->board);
    if (cloned->board == NULL)
    {
        free(cloned);
        return NULL;
    }

    cloned->constraints = hpp_validation_constraints_create(sudoku->constraints->side_length,
                                                            sudoku->constraints->block_length);
    if (cloned->constraints == NULL)
    {
        destroy_board(&cloned->board);
        free(cloned);
        return NULL;
    }

    if (!hpp_validation_constraints_init_from_board(cloned->constraints, cloned->board))
    {
        hpp_validation_constraints_destroy(&cloned->constraints);
        destroy_board(&cloned->board);
        free(cloned);
        return NULL;
    }

    return cloned;
}

void hpp_sudoku_destroy(hpp_sudoku** sudoku)
{
    if (sudoku == NULL || *sudoku == NULL)
    {
        return;
    }

    hpp_sudoku* s = *sudoku;
    destroy_board(&s->board);
    hpp_validation_constraints_destroy(&s->constraints);
    free(s);
    *sudoku = NULL;
}

bool hpp_sudoku_set_cell(hpp_sudoku* sudoku, size_t row, size_t col, hpp_cell value)
{
    if (sudoku == NULL || row >= sudoku->board->side_length || col >= sudoku->board->side_length)
    {
        return false;
    }

    // Reject out-of-range values early.
    if (value != BOARD_CELL_EMPTY && value > sudoku->board->side_length)
    {
        return false;
    }

    // Validate placement before mutating state.
    if (value != BOARD_CELL_EMPTY && !hpp_sudoku_can_place_value(sudoku, row, col, value))
    {
        return false;
    }

    size_t   index     = (row * sudoku->board->side_length) + col;
    hpp_cell old_value = sudoku->board->cells[index];

    // Remove old value from constraints if the cell was assigned.
    if (old_value != BOARD_CELL_EMPTY)
    {
        hpp_validation_row_remove_value(sudoku->constraints, row, old_value);
        hpp_validation_col_remove_value(sudoku->constraints, col, old_value);
        hpp_validation_box_remove_value(sudoku->constraints, row, col, old_value);
    }

    // Apply new cell value.
    sudoku->board->cells[index] = value;

    // Add new value to constraints if assigned.
    if (value != BOARD_CELL_EMPTY)
    {
        hpp_validation_row_add_value(sudoku->constraints, row, value);
        hpp_validation_col_add_value(sudoku->constraints, col, value);
        hpp_validation_box_add_value(sudoku->constraints, row, col, value);
    }

    return true;
}

void hpp_sudoku_clear_cell(hpp_sudoku* sudoku, size_t row, size_t col)
{
    if (sudoku == NULL || row >= sudoku->board->side_length || col >= sudoku->board->side_length)
    {
        return;
    }

    size_t   index     = (row * sudoku->board->side_length) + col;
    hpp_cell old_value = sudoku->board->cells[index];

    if (old_value != BOARD_CELL_EMPTY)
    {
        hpp_validation_row_remove_value(sudoku->constraints, row, old_value);
        hpp_validation_col_remove_value(sudoku->constraints, col, old_value);
        hpp_validation_box_remove_value(sudoku->constraints, row, col, old_value);
        sudoku->board->cells[index] = BOARD_CELL_EMPTY;
    }
}

bool hpp_sudoku_can_place_value(const hpp_sudoku* sudoku, size_t row, size_t col, hpp_cell value)
{
    if (sudoku == NULL || value == BOARD_CELL_EMPTY || value > sudoku->board->side_length)
    {
        return false;
    }

    return hpp_validation_can_place_value(sudoku->constraints, row, col, value);
}

bool hpp_sudoku_is_valid(const hpp_sudoku* sudoku)
{
    if (sudoku == NULL || sudoku->board == NULL || sudoku->constraints == NULL)
    {
        return false;
    }

    // Validate board shape invariants.
    if (sudoku->board->cells == NULL || sudoku->board->side_length == 0 ||
        sudoku->board->block_length == 0 ||
        sudoku->board->cell_count != sudoku->board->side_length * sudoku->board->side_length)
    {
        return false;
    }

    if (sudoku->board->block_length * sudoku->board->block_length != sudoku->board->side_length)
    {
        return false;
    }

    // Verify constraints reflect all assigned board values.
    for (size_t row = 0; row < sudoku->board->side_length; ++row)
    {
        for (size_t col = 0; col < sudoku->board->side_length; ++col)
        {
            size_t   index = (row * sudoku->board->side_length) + col;
            hpp_cell value = sudoku->board->cells[index];

            if (value == BOARD_CELL_EMPTY)
            {
                continue;
            }

            // Value must be in the legal symbol range.
            if (value > sudoku->board->side_length)
            {
                return false;
            }

            // Value must be present in every corresponding unit bitvector.
            if (!hpp_validation_row_has_value(sudoku->constraints, row, value) ||
                !hpp_validation_col_has_value(sudoku->constraints, col, value) ||
                !hpp_validation_box_has_value(sudoku->constraints, row, col, value))
            {
                return false;
            }
        }
    }

    return true;
}

/* =========================================================================
 * Internal Helpers
 * ========================================================================= */

/**
 * @brief Infer square block length from side length.
 *
 * @note Uses an integer monotonic scan (`1,2,3,...`) instead of floating
 *       point math to avoid rounding issues and keep behavior deterministic.
 * @pre `side_length > 0` for meaningful Sudoku dimensions.
 * @post Returns either an exact integer root or `0` for non-square values.
 *
 * @param side_length Board side length to evaluate.
 * @return Inferred block length, or `0` when no integral square root exists.
 */
static size_t hpp_sudoku_infer_block_length(size_t side_length)
{
    size_t block_length = 1;
    while (block_length * block_length < side_length)
    {
        block_length++;
    }

    return (block_length * block_length == side_length) ? block_length : 0;
}
