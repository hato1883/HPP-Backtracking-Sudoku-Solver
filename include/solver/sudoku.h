#ifndef SOLVER_SUDOKU_H
#define SOLVER_SUDOKU_H

#include "utils/bitvector.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

enum
{
    SUDOKU_EMPTY = 0, // Reserved due to range [0, 255]
};

/**
 * Representation of a cell in the sudoku board
 * [0, 255]
 */
typedef uint8_t hpp_cell;

/**
 * Representation of a sudoku board
 * with a square dimension of size x size
 * TODO: Change to 1D matrix if pointer chasing is expensive
 */
typedef struct Board
{
    size_t       size;
    size_t       block_size;
    size_t       masks;
    hpp_cell**   cells;
    hpp_bitmask* fixed;
} hpp_board;

/**
 * Creates a new empty board with garabage values of size: size x size
 *
 * @param size the size of the board in 1 dimennsion, all boards are square.
 * @return A pointer to newly created board allocated on the heap
 */
hpp_board* create_board(size_t size);

/**
 * Destroys a heap allocated board and set the pointer to NULL
 * @param board pointer to the board refrence you want to destroy.
 */
void destroy_board(hpp_board** board);

/**
 * Prints the board to stdout
 * @param board the board refrence you want to print
 */
void print_board(hpp_board* board);

/**
 * Prints the bitmasked to mark hint/unknown to stdout
 * @param board the board refrence you want to print
 */
void print_bitmasks(hpp_board* board);

/**
 * Randomize unfixed cells in the board by filling row-wise.
 * Each row is guaranteed to contain all digits from 1 to size exactly once.
 * Fixed cells (hints) are never modified.
 *
 * @param board the board to randomize
 */
void randomize_board(hpp_board* board);

/**
 * Count the number of conflicts (duplicate values) in a specific row.
 * @param board the board to check
 * @param row the row index
 * @return the number of conflicts found in that row
 */
static inline size_t count_conflicts_in_row(const hpp_board* board, size_t row)
{
    size_t conflicts = 0;
    for (size_t col1 = 0; col1 < board->size; ++col1)
    {
        uint8_t val1 = board->cells[row][col1];
        for (size_t col2 = col1 + 1; col2 < board->size; ++col2)
        {
            if (board->cells[row][col2] == val1)
            {
                conflicts++;
            }
        }
    }
    return conflicts;
}

/**
 * Count the number of conflicts (duplicate values) in a specific column.
 * @param board the board to check
 * @param col the column index
 * @return the number of conflicts found in that column
 */
static inline size_t count_conflicts_in_col(const hpp_board* board, size_t col)
{
    size_t conflicts = 0;
    for (size_t row1 = 0; row1 < board->size; ++row1)
    {
        uint8_t val1 = board->cells[row1][col];
        for (size_t row2 = row1 + 1; row2 < board->size; ++row2)
        {
            if (board->cells[row2][col] == val1)
            {
                conflicts++;
            }
        }
    }
    return conflicts;
}

/**
 * Count the number of conflicts (duplicate values) in a specific block.
 * @param board the board to check
 * @param block_row the block row index (0 to board->size/board->block_size - 1)
 * @param block_col the block column index (0 to board->size/board->block_size - 1)
 * @return the number of conflicts found in that block
 */
static inline size_t
count_conflicts_in_block(const hpp_board* board, size_t block_row, size_t block_col)
{
    size_t conflicts = 0;
    size_t start_row = block_row * board->block_size;
    size_t start_col = block_col * board->block_size;

    for (size_t i1 = 0; i1 < board->block_size; ++i1)
    {
        for (size_t j1 = 0; j1 < board->block_size; ++j1)
        {
            uint8_t val1 = board->cells[start_row + i1][start_col + j1];

            // Check against remaining cells in block
            for (size_t i2 = i1; i2 < board->block_size; ++i2)
            {
                size_t j2_start = (i2 == i1) ? j1 + 1 : 0;
                for (size_t j2 = j2_start; j2 < board->block_size; ++j2)
                {
                    if (board->cells[start_row + i2][start_col + j2] == val1)
                    {
                        conflicts++;
                    }
                }
            }
        }
    }
    return conflicts;
}

#endif