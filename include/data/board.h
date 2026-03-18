/**
 * @file data/board.h
 * @brief Flat Sudoku board container and lifecycle helpers.
 *
 * `hpp_board` stores values in row-major order:
 * `index = row * side_length + col`.
 */

#ifndef DATA_BOARD_H
#define DATA_BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* =========================================================================
 * Cell Model
 * ========================================================================= */

/** Special value used for unassigned cells. */
enum BoardValues
{
    BOARD_CELL_EMPTY = 0,
};

/**
 * Cell value type.
 *
 * Values are represented as bytes (`uint8_t`), so practical board dimensions
 * are constrained to fit in that domain.
 */
typedef uint8_t hpp_cell;

/**
 * @brief Sudoku board in row-major flat storage.
 */
typedef struct Board
{
    size_t    side_length;  /**< Number of rows/columns (e.g., 9 for a 9x9 board). */
    size_t    block_length; /**< Sub-grid side length (e.g., 3 for a 9x9 board).    */
    size_t    cell_count;   /**< Total number of cells (`side_length * side_length`). */
    hpp_cell* cells;        /**< Flat row-major cell buffer of length `cell_count`.    */
} hpp_board;

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

/**
 * @brief Allocate a new empty board.
 *
 * `size` must be a perfect square (e.g., 4, 9, 16).
 *
 * @note Allocated cells are zero-initialized (`BOARD_CELL_EMPTY`).
 * @pre `size > 0` and `size` is a perfect square.
 * @post Returned board owns a `cell_count == size * size` cell buffer.
 *
 * @param size Board side length.
 * @return Newly allocated board, or `NULL` if allocation fails.
 *
 * @par Example
 * @code{.c}
 * hpp_board* board = create_board(9);
 * if (board == NULL) {
 *     return 1;
 * }
 * destroy_board(&board);
 * @endcode
 */
hpp_board* create_board(size_t size);

/**
 * @brief Deep-clone an existing board.
 *
 * @note Clone owns an independent cell buffer.
 * @pre `source != NULL` and `source->cells != NULL`.
 * @post On success, clone dimensions and cell values match `source`.
 *
 * @param source Source board to clone.
 * @return Newly allocated clone, or `NULL` on allocation failure.
 */
hpp_board* hpp_board_clone(const hpp_board* source);

/**
 * @brief Copy all board data from `source` into `destination`.
 *
 * @note This is an in-place overwrite of destination cells.
 * @pre `destination` and `source` are non-NULL and dimension-compatible.
 * @post On success, `destination` cells equal `source` cells.
 *
 * @param destination Destination board (must match source dimensions).
 * @param source Source board.
 * @return `true` when copy succeeds.
 */
bool hpp_board_copy(hpp_board* destination, const hpp_board* source);

/**
 * @brief Destroy a board and set the caller pointer to `NULL`.
 *
 * @note Passing `NULL` board storage is invalid in this implementation.
 * @pre `board != NULL` and `*board` points to a live board.
 * @post Board memory is released and `*board == NULL`.
 *
 * @param board Address of board pointer to destroy.
 */
void destroy_board(hpp_board** board);

/* =========================================================================
 * Utilities
 * ========================================================================= */

/**
 * @brief Print the board to stdout in a human-readable grid form.
 *
 * @note Empty cells are rendered as `.` placeholders.
 * @pre `board != NULL` and `board->cells != NULL`.
 * @post Board contents are unchanged.
 *
 * @param board Board to print.
 */
void print_board(const hpp_board* board);

#endif /* DATA_BOARD_H */