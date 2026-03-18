/**
 * @file solver/validation.h
 * @brief Bitvector-backed row/column/box constraint tracking.
 *
 * Validation is implemented as three sets of bitvectors (rows, columns, boxes)
 * for O(1) membership checks during solving.
 */

#ifndef DATA_VALIDATION_H
#define DATA_VALIDATION_H

#include "data/bitvector.h"
#include "data/board.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* =========================================================================
 * Data Model
 * ========================================================================= */

/**
 * @brief Constraint bitvectors for rows, columns, and boxes.
 */
typedef struct ValidationConstraints
{
    hpp_bitvector_word* row_bitvectors; /**< Flat buffer: row-major unit order. */
    hpp_bitvector_word* col_bitvectors; /**< Flat buffer: column-major unit order. */
    hpp_bitvector_word* box_bitvectors; /**< Flat buffer: box-major unit order. */

    size_t side_length;  /**< Board side length (N). */
    size_t block_length; /**< Sub-grid side length.  */
} hpp_validation_constraints;

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

/**
 * @brief Allocate constraint storage for a board shape.
 *
 * @note Bitvectors track values in the `0..255` domain.
 * @pre `side_length > 0` and `block_length > 0`.
 * @post On success, all occupancy bitvectors are zero-initialized.
 *
 * @param side_length Board side length.
 * @param block_length Sub-grid side length.
 * @return Newly allocated constraints object, or `NULL` on allocation failure.
 *
 * @par Example
 * @code{.c}
 * hpp_validation_constraints* c = hpp_validation_constraints_create(9, 3);
 * if (c != NULL) {
 *     hpp_validation_constraints_destroy(&c);
 * }
 * @endcode
 */
hpp_validation_constraints* hpp_validation_constraints_create(size_t side_length,
                                                              size_t block_length);

/**
 * @brief Destroy constraints and set the caller pointer to `NULL`.
 *
 * @pre `constraints != NULL`.
 * @post If pointer was valid, `*constraints == NULL`.
 *
 * @param constraints Address of constraints pointer to destroy.
 */
void hpp_validation_constraints_destroy(hpp_validation_constraints** constraints);

/**
 * @brief Rebuild constraints from a board snapshot.
 *
 * @note Returns `false` when duplicate placements exist in any unit.
 * @pre `constraints != NULL` and `board != NULL`.
 * @post On success, bitvectors exactly reflect assigned board values.
 *
 * @param constraints Constraints object to initialize.
 * @param board Board snapshot to read from.
 * @return `false` if the board already violates Sudoku constraints.
 */
bool hpp_validation_constraints_init_from_board(hpp_validation_constraints* constraints,
                                                const hpp_board*            board);

/* =========================================================================
 * Row Operations
 * ========================================================================= */

/**
 * @brief Add one value to a row occupancy bitvector.
 *
 * @pre `constraints != NULL` and `row` is in range.
 * @post On success, row bitvector includes `value`.
 *
 * @param constraints Constraints object.
 * @param row Row index.
 * @param value Value to mark present.
 * @return `false` if value is already present.
 */
bool hpp_validation_row_add_value(hpp_validation_constraints* constraints,
                                  size_t                      row,
                                  size_t                      value);

/**
 * @brief Remove one value from a row occupancy bitvector.
 *
 * @pre `constraints != NULL` and `row` is in range.
 * @post Row bitvector no longer includes `value`.
 *
 * @param constraints Constraints object.
 * @param row Row index.
 * @param value Value to clear.
 */
void hpp_validation_row_remove_value(hpp_validation_constraints* constraints,
                                     size_t                      row,
                                     size_t                      value);

/**
 * @brief Check row occupancy for one value.
 *
 * @pre `constraints != NULL` and `row` is in range.
 * @post Constraint state is unchanged.
 *
 * @param constraints Constraints object.
 * @param row Row index.
 * @param value Value to test.
 * @return `true` if value is already present in row.
 */
bool hpp_validation_row_has_value(const hpp_validation_constraints* constraints,
                                  size_t                            row,
                                  size_t                            value);

/* =========================================================================
 * Column Operations
 * ========================================================================= */

/**
 * @brief Add one value to a column occupancy bitvector.
 *
 * @pre `constraints != NULL` and `col` is in range.
 * @post On success, column bitvector includes `value`.
 *
 * @param constraints Constraints object.
 * @param col Column index.
 * @param value Value to mark present.
 * @return `false` if value is already present.
 */
bool hpp_validation_col_add_value(hpp_validation_constraints* constraints,
                                  size_t                      col,
                                  size_t                      value);

/**
 * @brief Remove one value from a column occupancy bitvector.
 *
 * @pre `constraints != NULL` and `col` is in range.
 * @post Column bitvector no longer includes `value`.
 *
 * @param constraints Constraints object.
 * @param col Column index.
 * @param value Value to clear.
 */
void hpp_validation_col_remove_value(hpp_validation_constraints* constraints,
                                     size_t                      col,
                                     size_t                      value);

/**
 * @brief Check column occupancy for one value.
 *
 * @pre `constraints != NULL` and `col` is in range.
 * @post Constraint state is unchanged.
 *
 * @param constraints Constraints object.
 * @param col Column index.
 * @param value Value to test.
 * @return `true` if value is already present in column.
 */
bool hpp_validation_col_has_value(const hpp_validation_constraints* constraints,
                                  size_t                            col,
                                  size_t                            value);

/* =========================================================================
 * Box Operations
 * ========================================================================= */

/**
 * @brief Add one value to a box occupancy bitvector.
 *
 * @pre `constraints != NULL` and coordinates are in range.
 * @post On success, target box bitvector includes `value`.
 *
 * @param constraints Constraints object.
 * @param row Row index.
 * @param col Column index.
 * @param value Value to mark present.
 * @return `false` if value is already present.
 */
bool hpp_validation_box_add_value(hpp_validation_constraints* constraints,
                                  size_t                      row,
                                  size_t                      col,
                                  size_t                      value);

/**
 * @brief Remove one value from a box occupancy bitvector.
 *
 * @pre `constraints != NULL` and coordinates are in range.
 * @post Target box bitvector no longer includes `value`.
 *
 * @param constraints Constraints object.
 * @param row Row index.
 * @param col Column index.
 * @param value Value to clear.
 */
void hpp_validation_box_remove_value(hpp_validation_constraints* constraints,
                                     size_t                      row,
                                     size_t                      col,
                                     size_t                      value);

/**
 * @brief Check box occupancy for one value.
 *
 * @pre `constraints != NULL` and coordinates are in range.
 * @post Constraint state is unchanged.
 *
 * @param constraints Constraints object.
 * @param row Row index.
 * @param col Column index.
 * @param value Value to test.
 * @return `true` if value is already present in the box.
 */
bool hpp_validation_box_has_value(const hpp_validation_constraints* constraints,
                                  size_t                            row,
                                  size_t                            col,
                                  size_t                            value);

/* =========================================================================
 * Composite Validation
 * ========================================================================= */

/**
 * @brief Check whether `value` can be placed at `(row, col)`.
 *
 * @note This helper is read-only and does not mutate constraints.
 * @pre `constraints != NULL` and coordinates are in range.
 * @post Constraint state is unchanged.
 *
 * @param constraints Constraints object.
 * @param row Row index.
 * @param col Column index.
 * @param value Candidate value.
 * @return `false` if any row/column/box conflict exists.
 *
 * @par Example
 * @code{.c}
 * if (hpp_validation_can_place_value(constraints, row, col, 7)) {
 *     (void)hpp_validation_row_add_value(constraints, row, 7);
 * }
 * @endcode
 */
bool hpp_validation_can_place_value(const hpp_validation_constraints* constraints,
                                    size_t                            row,
                                    size_t                            col,
                                    size_t                            value);

#endif /* DATA_VALIDATION_H */
