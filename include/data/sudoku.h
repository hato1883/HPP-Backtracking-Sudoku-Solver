/**
 * @file data/sudoku.h
 * @brief High-level Sudoku state container (board + constraints).
 *
 * `hpp_sudoku` couples a mutable board with its validation layer so callers
 * can update cells while maintaining row/column/box consistency.
 */

#ifndef DATA_SUDOKU_H
#define DATA_SUDOKU_H

#include "data/board.h"
#include "data/validation.h"

#include <stdbool.h>
#include <stddef.h>

/* =========================================================================
 * Data Model
 * ========================================================================= */

/**
 * @brief Combined Sudoku state.
 */
typedef struct Sudoku
{
    hpp_board* board; /**< Board values in row-major order. */
    hpp_validation_constraints*
        constraints; /**< Validation bitvectors synchronized with `board`. */
} hpp_sudoku;

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

/**
 * @brief Create an empty Sudoku with the provided dimensions.
 *
 * @note Constraint caches are allocated together with the board.
 * @pre `side_length > 0` and `block_length > 0`.
 * @post On success, all cells are empty and constraints are initialized.
 *
 * @param side_length Number of rows/columns.
 * @param block_length Sub-grid side length.
 * @return Newly allocated Sudoku instance, or `NULL` on failure.
 *
 * @par Example
 * @code{.c}
 * hpp_sudoku* sudoku = hpp_sudoku_create(9, 3);
 * if (sudoku == NULL) {
 *     return 1;
 * }
 * hpp_sudoku_destroy(&sudoku);
 * @endcode
 */
hpp_sudoku* hpp_sudoku_create(size_t side_length, size_t block_length);

/**
 * @brief Create a Sudoku from an existing board snapshot.
 *
 * @note The board is cloned; caller retains ownership of `board`.
 * @pre `board != NULL` and board dimensions are internally valid.
 * @post On success, returned Sudoku reflects the same assignments as `board`.
 *
 * @param board Source board snapshot.
 * @return A new Sudoku instance, or `NULL` on allocation/validation failure.
 */
hpp_sudoku* hpp_sudoku_create_from_board(const hpp_board* board);

/**
 * @brief Deep-clone a Sudoku (board + constraints).
 *
 * @note Clone is fully independent from source.
 * @pre `sudoku != NULL`.
 * @post On success, clone state matches source state.
 *
 * @param sudoku Source Sudoku state.
 * @return Newly allocated clone, or `NULL` on failure.
 */
hpp_sudoku* hpp_sudoku_clone(const hpp_sudoku* sudoku);

/**
 * @brief Destroy a Sudoku and set the caller pointer to `NULL`.
 *
 * @pre `sudoku != NULL`.
 * @post If input pointer was valid, `*sudoku == NULL`.
 *
 * @param sudoku Address of Sudoku pointer to destroy.
 */
void hpp_sudoku_destroy(hpp_sudoku** sudoku);

/* =========================================================================
 * Mutations and Validation
 * ========================================================================= */

/**
 * @brief Set a cell and update all constraint structures.
 *
 * @note Setting `BOARD_CELL_EMPTY` clears the cell.
 * @pre `sudoku != NULL`, `row`/`col` are in range, and `value` is in domain.
 * @post On success, board value and constraint bitvectors are synchronized.
 *
 * @param sudoku Sudoku state to mutate.
 * @param row Row index.
 * @param col Column index.
 * @param value Value to place (or `BOARD_CELL_EMPTY`).
 * @return `false` if coordinates/value are invalid or constraints would break.
 *
 * @par Example
 * @code{.c}
 * if (!hpp_sudoku_set_cell(sudoku, 0, 0, 5)) {
 *     // handle conflict
 * }
 * @endcode
 */
bool hpp_sudoku_set_cell(hpp_sudoku* sudoku, size_t row, size_t col, hpp_cell value);

/**
 * @brief Clear a cell to `BOARD_CELL_EMPTY` and remove related constraints.
 *
 * @pre `sudoku != NULL` and coordinates are in range.
 * @post Target cell is empty and constraints no longer include its old value.
 *
 * @param sudoku Sudoku state to mutate.
 * @param row Row index.
 * @param col Column index.
 */
void hpp_sudoku_clear_cell(hpp_sudoku* sudoku, size_t row, size_t col);

/**
 * @brief Check whether `value` can be placed at `(row, col)`.
 *
 * @note This function does not mutate state.
 * @pre `sudoku != NULL` and `value != BOARD_CELL_EMPTY`.
 * @post Sudoku state remains unchanged.
 *
 * @param sudoku Sudoku state.
 * @param row Row index.
 * @param col Column index.
 * @param value Candidate value.
 * @return `true` if placement is currently valid.
 */
bool hpp_sudoku_can_place_value(const hpp_sudoku* sudoku, size_t row, size_t col, hpp_cell value);

/**
 * @brief Validate internal consistency between board state and constraints.
 *
 * @note Intended for assertions/tests and defensive checks.
 * @pre `sudoku != NULL`.
 * @post Sudoku state remains unchanged.
 *
 * @param sudoku Sudoku state to validate.
 * @return `true` if state is internally consistent.
 *
 * @par Example
 * @code{.c}
 * if (!hpp_sudoku_is_valid(sudoku)) {
 *     fprintf(stderr, "sudoku state is inconsistent\n");
 * }
 * @endcode
 */
bool hpp_sudoku_is_valid(const hpp_sudoku* sudoku);

#endif /* DATA_SUDOKU_H */
