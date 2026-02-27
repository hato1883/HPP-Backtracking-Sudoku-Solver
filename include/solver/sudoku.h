#ifndef __SUDOKU_H__

#include <stdlib.h>

/**
 * Representation of a Number in the sudoku board
 * [0, 255]
 */
typedef unsigned char Number;

/**
 * Representation of a sudoku board
 * with a square dimension of size x size
 * TODO: Change to 1D matrix if pointer chasing is expensive
 */
typedef struct Board
{
    size_t   size;
    Number** board;
} Board;

/**
 * Creates a new empty board with garabage values of size: size x size
 * 
 * @param size the size of the board in 1 dimennsion, all boards are square.
 * @return A pointer to newly created board allocated on the heap
 */
Board* create_board(size_t size);

/**
 * Destroys a heap allocated board and set the pointer to NULL
 * @param board pointer to the board you want to destroy.
 */
void destroy_board(Board **board);

#endif