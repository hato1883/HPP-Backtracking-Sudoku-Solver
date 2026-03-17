#include "data/sudoku.h"

#include <stdlib.h>

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

    // Determine block_length from board (assuming standard Sudoku pattern)
    // For a 9x9 board, block_length = 3; for 16x16, block_length = 4, etc.
    size_t block_length = 1;
    while (block_length * block_length < board->side_length)
    {
        block_length++;
    }

    sudoku->constraints = hpp_validation_constraints_create(board->side_length, block_length);
    if (sudoku->constraints == NULL)
    {
        destroy_board(&sudoku->board);
        free(sudoku);
        return NULL;
    }

    // Initialize constraints from board
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

    // Check if value is valid
    if (value != BOARD_CELL_EMPTY && value > sudoku->board->side_length)
    {
        return false;
    }

    // Check for conflicts if placing a non-empty value
    if (value != BOARD_CELL_EMPTY && !hpp_sudoku_can_place_value(sudoku, row, col, value))
    {
        return false;
    }

    size_t   index     = (row * sudoku->board->side_length) + col;
    hpp_cell old_value = sudoku->board->cells[index];

    // Remove old value from constraints if it exists
    if (old_value != BOARD_CELL_EMPTY)
    {
        hpp_validation_row_remove_value(sudoku->constraints, row, old_value);
        hpp_validation_col_remove_value(sudoku->constraints, col, old_value);
        hpp_validation_box_remove_value(sudoku->constraints, row, col, old_value);
    }

    // Set the new value
    sudoku->board->cells[index] = value;

    // Add new value to constraints if it's not empty
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

    // Check board structure
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

    // Verify constraints match board state
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

            // Value must be valid range
            if (value > sudoku->board->side_length)
            {
                return false;
            }

            // Value must be in constraints
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
