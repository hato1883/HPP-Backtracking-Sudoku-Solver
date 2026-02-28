#ifndef SOLVER_SUDOKU_H
#define SOLVER_SUDOKU_H

#include <stdint.h>
#include <stdlib.h>

enum
{
    SUDOKU_EMPTY = 0,
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
    size_t     size;
    hpp_cell** cells;
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

#endif