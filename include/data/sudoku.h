#ifndef DATA_SUDOKU_H
#define DATA_SUDOKU_H

#include "data/board.h"
#include "data/validation.h"

#include <stdbool.h>

/**
 * Sudoku game state combining board and validation constraints.
 * This module encapsulates the board state along with associated constraint
 * information (bitvector-based validation).
 */
typedef struct Sudoku
{
    hpp_board*                  board;
    hpp_validation_constraints* constraints;
} hpp_sudoku;

/**
 * Create a Sudoku instance with an empty board.
 */
hpp_sudoku* hpp_sudoku_create(size_t side_length, size_t block_length);

/**
 * Create a Sudoku instance from an existing board.
 * The constraints are initialized based on the board state.
 * Returns NULL if initialization fails.
 */
hpp_sudoku* hpp_sudoku_create_from_board(const hpp_board* board);

/**
 * Clone a Sudoku instance (board + constraints).
 */
hpp_sudoku* hpp_sudoku_clone(const hpp_sudoku* sudoku);

/**
 * Destroy a Sudoku instance and free all memory.
 */
void hpp_sudoku_destroy(hpp_sudoku** sudoku);

/**
 * Set a cell value and update constraints.
 * Returns false if the value conflicts with existing constraints.
 */
bool hpp_sudoku_set_cell(hpp_sudoku* sudoku, size_t row, size_t col, hpp_cell value);

/**
 * Clear a cell (set to BOARD_CELL_EMPTY) and remove from constraints.
 */
void hpp_sudoku_clear_cell(hpp_sudoku* sudoku, size_t row, size_t col);

/**
 * Check if a value can be placed at (row, col) using constraint validation.
 */
bool hpp_sudoku_can_place_value(const hpp_sudoku* sudoku, size_t row, size_t col, hpp_cell value);

/**
 * Validate the entire Sudoku state.
 */
bool hpp_sudoku_is_valid(const hpp_sudoku* sudoku);

#endif
