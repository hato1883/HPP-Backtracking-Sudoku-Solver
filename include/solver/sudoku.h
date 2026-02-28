#ifndef SOLVER_SUDOKU_H
#define SOLVER_SUDOKU_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

enum
{
    SUDOKU_EMPTY          = 0,
    BIT_LOCKED            = 1,
    LAST_BIT_IN_VECTOR    = 63,
    CELLS_PER_VECTOR      = 64,
    FIXED_MASK_WORD_SHIFT = 6,
};

/**
 * Representation of a cell in the sudoku board
 * [0, 255]
 */
typedef uint8_t  hpp_cell;
typedef uint64_t hpp_bitmask;

/**
 * Representation of a sudoku board
 * with a square dimension of size x size
 * TODO: Change to 1D matrix if pointer chasing is expensive
 */
typedef struct Board
{
    size_t       size;
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

//
// Bitmaks methods below
//

/**
 * Converts the 2d index into a 1d index for usage in the bitmask
 * @param row row index in the 2d array
 * @param col col index in the 2d array
 * @param n size of the array (width or height)
 * @return index of bit in the bitmaks
 */
static inline size_t idx(size_t row, size_t col, size_t n)
{
    return (row * n) + col;
}

/**
 * Check if a cell is a hint or not.
 * @param mask the mask to check inside of
 * @param row row index in the 2d array
 * @param col col index in the 2d array
 * @param n size of the array (width or height)
 * @return true if the cell is a hint and can not be modified.
 */
static inline bool is_fixed(const hpp_bitmask* mask, size_t row, size_t col, size_t n)
{
    size_t linear = idx(row, col, n);
    return (mask[linear >> FIXED_MASK_WORD_SHIFT] >> (linear & LAST_BIT_IN_VECTOR)) & BIT_LOCKED;
}

/**
 * Check if a cell is a hint or not.
 * @param mask the mask to check inside of
 * @param linear_index a 1d index to use inside of the bitmask
 * @return true if the cell is a hint and can not be modified.
 */
static inline bool is_fixed_linear(const hpp_bitmask* mask, size_t linear)
{
    return (mask[linear >> FIXED_MASK_WORD_SHIFT] >> (linear & LAST_BIT_IN_VECTOR)) & BIT_LOCKED;
}

/**
 * Sets a entire bitvector of locks/unlocks
 * good when buiulding a new array
 *
 * @param mask the mask to modify inside of
 * @param new_mask the new value for a mask region
 * @param index the index of the region in the bitmask array
 */
static inline void bulk_set_mask(hpp_bitmask* mask, hpp_bitmask new_mask, size_t index)
{
    mask[index] = new_mask;
}

/**
 * sets a bit to locked or not
 *
 * @param mask the mask to check inside of
 * @param is_locked the value to set in the bitvector, true for locked
 * @param row row index in the 2d array
 * @param col col index in the 2d array
 * @param n size of the array (width or height)
 * @note this is inefficient and should instead be done in bulk
 */
static inline void set_bit(hpp_bitmask* mask, bool is_locked, size_t row, size_t col, size_t n)
{
    size_t linear   = idx(row, col, n);
    size_t word_idx = linear >> FIXED_MASK_WORD_SHIFT;
    size_t bit_pos  = linear & LAST_BIT_IN_VECTOR;
    // Clear the bit, then set it to the desired value
    mask[word_idx] = (mask[word_idx] & ~(1ULL << bit_pos)) | (is_locked << bit_pos);
}

/**
 * sets a bit to locked or not
 *
 * @param mask the mask to check inside of
 * @param is_locked the value to set in the bitvector, true for locked
 * @param linear_index a 1d index to use inside of the bitmask
 * @note this is inefficient and should instead be done in bulk
 */
static inline void set_bit_linear(hpp_bitmask* mask, hpp_bitmask is_locked, size_t linear)
{
    size_t word_idx = linear >> FIXED_MASK_WORD_SHIFT;
    size_t bit_pos  = linear & LAST_BIT_IN_VECTOR;
    // Clear the bit, then set it to the desired value
    mask[word_idx] = (mask[word_idx] & ~(1ULL << bit_pos)) | (is_locked << bit_pos);
}

#endif