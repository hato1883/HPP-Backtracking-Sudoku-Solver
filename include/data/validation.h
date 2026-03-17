#ifndef DATA_VALIDATION_H
#define DATA_VALIDATION_H

#include "data/board.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Bitvector-based validation structure for efficient constraint checking.
 * Uses bitmasking to track which values are present in rows, columns, and boxes.
 * Each value 0-255 maps to a single bit in the appropriate bitvector.
 */
typedef struct ValidationConstraints
{
    // Row constraints: for each row, track which values are present
    // size: side_length rows, each with 256 bits (32 uint8_t per row)
    uint8_t** row_bitvectors;

    // Column constraints: for each column, track which values are present
    // size: side_length columns, each with 256 bits (32 uint8_t per column)
    uint8_t** col_bitvectors;

    // Box constraints: for each box, track which values are present
    // size: (side_length / block_length)^2 boxes, each with 256 bits
    uint8_t** box_bitvectors;

    size_t side_length;
    size_t block_length;
} hpp_validation_constraints;

/**
 * Create and initialize validation constraints for a given board size.
 */
hpp_validation_constraints* hpp_validation_constraints_create(size_t side_length,
                                                              size_t block_length);

/**
 * Destroy validation constraints and free all memory.
 */
void hpp_validation_constraints_destroy(hpp_validation_constraints** constraints);

/**
 * Initialize validation constraints from an existing board state.
 * Returns false if initialization fails.
 */
bool hpp_validation_constraints_init_from_board(hpp_validation_constraints* constraints,
                                                const hpp_board*            board);

/**
 * Add a value to a row's constraint bitvector.
 * Early return with false if the value already exists (conflict detected).
 */
bool hpp_validation_row_add_value(hpp_validation_constraints* constraints,
                                  size_t                      row,
                                  size_t                      value);

/**
 * Remove a value from a row's constraint bitvector.
 */
void hpp_validation_row_remove_value(hpp_validation_constraints* constraints,
                                     size_t                      row,
                                     size_t                      value);

/**
 * Check if a value exists in a row's constraint bitvector.
 */
bool hpp_validation_row_has_value(const hpp_validation_constraints* constraints,
                                  size_t                            row,
                                  size_t                            value);

/**
 * Add a value to a column's constraint bitvector.
 * Early return with false if the value already exists (conflict detected).
 */
bool hpp_validation_col_add_value(hpp_validation_constraints* constraints,
                                  size_t                      col,
                                  size_t                      value);

/**
 * Remove a value from a column's constraint bitvector.
 */
void hpp_validation_col_remove_value(hpp_validation_constraints* constraints,
                                     size_t                      col,
                                     size_t                      value);

/**
 * Check if a value exists in a column's constraint bitvector.
 */
bool hpp_validation_col_has_value(const hpp_validation_constraints* constraints,
                                  size_t                            col,
                                  size_t                            value);

/**
 * Add a value to a box's constraint bitvector.
 * Early return with false if the value already exists (conflict detected).
 */
bool hpp_validation_box_add_value(hpp_validation_constraints* constraints,
                                  size_t                      row,
                                  size_t                      col,
                                  size_t                      value);

/**
 * Remove a value from a box's constraint bitvector.
 */
void hpp_validation_box_remove_value(hpp_validation_constraints* constraints,
                                     size_t                      row,
                                     size_t                      col,
                                     size_t                      value);

/**
 * Check if a value exists in a box's constraint bitvector.
 */
bool hpp_validation_box_has_value(const hpp_validation_constraints* constraints,
                                  size_t                            row,
                                  size_t                            col,
                                  size_t                            value);

/**
 * Validate if a value can be placed at (row, col) using bitmasking.
 * Returns false if the value conflicts with row, column, or box constraints.
 * Early termination on first conflict.
 */
bool hpp_validation_can_place_value(const hpp_validation_constraints* constraints,
                                    size_t                            row,
                                    size_t                            col,
                                    size_t                            value);

#endif
