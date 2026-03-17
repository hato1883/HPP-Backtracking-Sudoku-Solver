#include "data/board.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

hpp_board* create_board(size_t size)
{
    assert(size > 0 && "Board must be larger than 0");

    hpp_board* board    = malloc(sizeof(hpp_board));
    board->side_length  = size;
    board->block_length = (size_t)sqrtf(size);
    assert(board->block_length * board->block_length == board->side_length &&
           "Invalid board size!");

    board->cell_count = size * size;
    board->cells      = calloc(board->cell_count, sizeof(hpp_cell));
    assert(board->cells != NULL && "Memory allocation failed!");

    return board;
}

hpp_board* hpp_board_clone(const hpp_board* source)
{
    hpp_board* board    = malloc(sizeof(hpp_board));
    board->side_length  = source->side_length;
    board->block_length = source->block_length;
    board->cell_count   = source->cell_count;

    board->cells = malloc(board->cell_count * sizeof(hpp_cell));
    assert(board->cells != NULL && "Memory allocation failed!");

    memcpy(board->cells, source->cells, board->cell_count);

    return board;
}

bool hpp_board_copy(hpp_board* destination, const hpp_board* source)
{
    assert(destination->side_length == source->side_length && "Side size differs, can not copy");
    assert(destination->block_length == source->block_length && "Block size differs, can not copy");
    assert(destination->cell_count == source->cell_count && "Cell count differs, can not copy");
    destination->side_length  = source->side_length;
    destination->block_length = source->block_length;
    destination->cell_count   = source->cell_count;

    assert(destination->cells != NULL && "Invalid board state");
    assert(source->cells != NULL && "Invalid board state");
    memcpy(destination->cells, source->cells, destination->cell_count);

    return true;
}

void destroy_board(hpp_board** board_ptr)
{
    assert(board_ptr != NULL && "Can't destroy a NULL Pointer");

    hpp_board* board = *board_ptr;
    assert(board != NULL && "Board is already destroyed");

    assert(board->cells != NULL && "Board has a invalid state");
    free(board->cells);

    board->block_length = 0;
    board->cell_count   = 0;
    board->side_length  = 0;
    board->cells        = NULL;

    free(board);
    *board_ptr = NULL;
}

void print_board(const hpp_board* board)
{
    assert(board != NULL && "Invalid board");
    assert(board->cells != NULL && "Invalid board");

    const int    digits      = (int)log10((double)board->side_length) + 1;
    const size_t total_cells = board->side_length * board->side_length;

    for (size_t idx = 0; idx < total_cells; idx++)
    {
        const uint8_t cell = board->cells[idx];

        if (cell == BOARD_CELL_EMPTY)
        {
            for (int dig = 0; dig < digits; dig++)
            {
                printf(".");
            }
            printf(" ");
        }
        else
        {
            printf("%0*u ", digits, (unsigned)cell);
        }

        if ((idx + 1) % board->side_length == 0)
        {
            printf("\n");
        }
    }
}
