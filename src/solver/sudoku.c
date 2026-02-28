
#include "solver/sudoku.h"

#include "utils/logger.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

hpp_board* create_board(size_t size)
{
    assert(size > 0 && "Board Size must be larger than 0!");

    hpp_board* new_board = (hpp_board*)malloc(sizeof(hpp_board) * 1);
    new_board->size      = size;

    // Allocate row pointers
    new_board->cells = (hpp_cell**)malloc(sizeof(hpp_cell*) * size);
    if (!new_board->cells)
    {
        LOG_ERROR("Memory allocation failed during board creation!");
        exit(EXIT_FAILURE);
    }

    // Allocate each row
    for (size_t row = 0; row < size; row++)
    {
        new_board->cells[row] = (hpp_cell*)malloc(sizeof(hpp_cell) * size);
        if (!new_board->cells[row])
        {
            LOG_ERROR("Memory allocation failed during board creation!");
            exit(EXIT_FAILURE);
        }
    }

    return new_board;
}

void destroy_board(hpp_board** board)
{
    assert(board != NULL && "Invalid refrence to Board!");
    assert(*board != NULL && "Board is already destoyed!");

    hpp_board* to_destroy = *board;
    size_t     size       = to_destroy->size;

    for (size_t row = 0; row < size; row++)
    {
        free(to_destroy->cells[row]);
    }
    free((void*)to_destroy->cells);
    free(to_destroy);

    *board = NULL;
}

void print_board(hpp_board* board)
{
    int digits = (int)log10((double)board->size) + 1;

    for (size_t row = 0; row < board->size; row++)
    {
        for (size_t col = 0; col < board->size; col++)
        {
            if (board->cells[row][col] == SUDOKU_EMPTY)
            {
                for (int count = 0; count < digits; count++)
                {
                    printf(".");
                }
                printf(" ");
            }
            else
            {
                printf("%0*hhu ", digits, board->cells[row][col]);
            }
        }
        printf("\n");
    }
}
