
#include "solver/sudoku.h"

#include "utils/color.h"
#include "utils/logger.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

hpp_board* create_board(size_t size)
{
    assert(size > 0 && "Board Size must be larger than 0!");

    hpp_board* new_board  = (hpp_board*)malloc(sizeof(hpp_board) * 1);
    new_board->size       = size;
    new_board->block_size = 0; // Parser will compute and set this
    new_board->masks      = (size * size + MASK_WORD_STORAGE - 1) / MASK_WORD_STORAGE;

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

static void mark_used_values_in_row(const hpp_board* board, size_t row, bool* used)
{
    for (size_t col = 0; col < board->size; ++col)
    {
        if (is_fixed(board->fixed, row, col, board->size))
        {
            uint8_t value = board->cells[row][col];
            if (value > 0 && value <= (uint8_t)board->size)
            {
                used[value] = true;
            }
        }
    }
}

static size_t collect_missing_numbers(size_t size, const bool* used, uint8_t* missing)
{
    size_t missing_count = 0;
    for (size_t i = 1; i <= size; ++i)
    {
        if (!used[i])
        {
            missing[missing_count++] = (uint8_t)i;
        }
    }
    return missing_count;
}

static void shuffle_array(uint8_t* arr, size_t size)
{
    for (size_t i = size - 1; i > 0; --i)
    {
        size_t  rand_idx = (size_t)rand() % (i + 1);
        uint8_t temp     = arr[i];
        arr[i]           = arr[rand_idx];
        arr[rand_idx]    = temp;
    }
}

static void fill_unfixed_cells(hpp_board* board, size_t row, const uint8_t* missing)
{
    size_t missing_idx = 0;
    for (size_t col = 0; col < board->size; ++col)
    {
        if (!is_fixed(board->fixed, row, col, board->size))
        {
            board->cells[row][col] = missing[missing_idx++];
        }
    }
}

void randomize_board(hpp_board* board)
{
    // For each row, fill unfixed cells with missing numbers
    for (size_t row = 0; row < board->size; ++row)
    {
        // Track which numbers are already used in fixed cells of this row
        bool* used = (bool*)calloc(board->size + 1, sizeof(bool));
        if (used == NULL)
        {
            LOG_ERROR("Memory allocation failed in randomize_board!");
            exit(EXIT_FAILURE);
        }

        mark_used_values_in_row(board, row, used);

        // Collect missing numbers that need to be distributed in unfixed cells
        uint8_t* missing = (uint8_t*)calloc(board->size, sizeof(uint8_t));
        if (missing == NULL)
        {
            LOG_ERROR("Memory allocation failed in randomize_board!");
            exit(EXIT_FAILURE);
        }

        size_t missing_count = collect_missing_numbers(board->size, used, missing);

        // Fisher-Yates shuffle to randomize the missing numbers
        shuffle_array(missing, missing_count);

        // Fill unfixed cells with shuffled missing numbers
        fill_unfixed_cells(board, row, missing);

        free(used);
        free(missing);
    }
}
