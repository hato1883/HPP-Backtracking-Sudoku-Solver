
#include "solver/sudoku.h"

#include "utils/colors.h"
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
    new_board->masks     = (size * size + CELLS_PER_VECTOR - 1) / CELLS_PER_VECTOR;

    // Setup bitmask
    new_board->fixed = (hpp_bitmask*)calloc(new_board->masks, sizeof(hpp_bitmask));
    if (new_board->fixed == NULL)
    {
        LOG_ERROR("Memory allocation failed during board creation!");
        exit(EXIT_FAILURE);
    }

    // Allocate row pointers
    new_board->cells = (hpp_cell**)malloc(sizeof(hpp_cell*) * size);
    if (new_board->cells == NULL)
    {
        LOG_ERROR("Memory allocation failed during board creation!");
        exit(EXIT_FAILURE);
    }

    // Allocate each row
    for (size_t row = 0; row < size; row++)
    {
        new_board->cells[row] = (hpp_cell*)malloc(sizeof(hpp_cell) * size);
        if (new_board->cells[row] == NULL)
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

    // destroy 2d array of cells
    for (size_t row = 0; row < size; row++)
    {
        free(to_destroy->cells[row]);
    }
    free((void*)to_destroy->cells);

    // destroy bitmasks
    free((void*)to_destroy->fixed);

    // destroy struct
    free(to_destroy);

    // Safeguard
    *board = NULL;
}

void print_board(hpp_board* board)
{
    int    digits = (int)log10((double)board->size) + 1;
    size_t linear = 0;

    for (size_t row = 0; row < board->size; row++)
    {
        for (size_t col = 0; col < board->size; col++)
        {
            int         is_fixed = is_fixed_linear(board->fixed, linear);
            const char* color    = is_fixed ? COLOR_RED : COLOR_GREEN;
            // Add color
            printf("%s", color);

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

            // Remove color
            printf(COLOR_RESET);
            linear++;
        }
        printf("\n");
    }
}

void print_bitmasks(hpp_board* board)
{
    size_t linear = 0;

    for (size_t row = 0; row < board->size; row++)
    {
        for (size_t col = 0; col < board->size; col++)
        {
            int         is_fixed = is_fixed_linear(board->fixed, linear);
            const char* color    = is_fixed ? COLOR_RED : COLOR_GREEN;
            // Add color
            printf("%s", color);
            printf("%u ", is_fixed);

            // Remove color
            printf(COLOR_RESET);
            linear++;
        }
        printf("\n");
    }
}
