/**
 * @file solver/validation.c
 * @brief Bitvector-backed row/column/box constraint operations.
 */

#include "solver/validation.h"

#include "data/bitvector_ops.h"

#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Forward Declarations
 * ========================================================================= */

static size_t                     hpp_validation_box_count(size_t side_length, size_t block_length);
static size_t                     hpp_get_box_index(size_t row, size_t col, size_t block_length);
static inline hpp_bitvector_word* hpp_row_bits_mut(hpp_validation_constraints* constraints,
                                                   size_t                      row);
static inline const hpp_bitvector_word* hpp_row_bits(const hpp_validation_constraints* constraints,
                                                     size_t                            row);
static inline hpp_bitvector_word*       hpp_col_bits_mut(hpp_validation_constraints* constraints,
                                                         size_t                      col);
static inline const hpp_bitvector_word* hpp_col_bits(const hpp_validation_constraints* constraints,
                                                     size_t                            col);
static inline hpp_bitvector_word*       hpp_box_bits_mut(hpp_validation_constraints* constraints,
                                                         size_t                      box_idx);
static inline const hpp_bitvector_word* hpp_box_bits(const hpp_validation_constraints* constraints,
                                                     size_t                            box_idx);

/* =========================================================================
 * Public API
 * ========================================================================= */

hpp_validation_constraints* hpp_validation_constraints_create(size_t side_length,
                                                              size_t block_length)
{
    hpp_validation_constraints* constraints = calloc(1, sizeof(hpp_validation_constraints));
    if (constraints == NULL)
    {
        return NULL;
    }

    constraints->side_length  = side_length;
    constraints->block_length = block_length;

    const size_t box_count = hpp_validation_box_count(side_length, block_length);

    // Flat 1D storage ordered by row index.
    constraints->row_bitvectors = calloc(side_length, HPP_BITVECTOR_BYTES);
    if (constraints->row_bitvectors == NULL)
    {
        hpp_validation_constraints_destroy(&constraints);
        return NULL;
    }

    // Flat 1D storage ordered by column index.
    constraints->col_bitvectors = calloc(side_length, HPP_BITVECTOR_BYTES);
    if (constraints->col_bitvectors == NULL)
    {
        hpp_validation_constraints_destroy(&constraints);
        return NULL;
    }

    // Flat 1D storage ordered by box index (box rows first, then box columns).
    constraints->box_bitvectors = calloc(box_count, HPP_BITVECTOR_BYTES);
    if (constraints->box_bitvectors == NULL)
    {
        hpp_validation_constraints_destroy(&constraints);
        return NULL;
    }

    return constraints;
}

void hpp_validation_constraints_destroy(hpp_validation_constraints** constraints)
{
    if (constraints == NULL || *constraints == NULL)
    {
        return;
    }

    hpp_validation_constraints* c = *constraints;

    free(c->row_bitvectors);
    free(c->col_bitvectors);
    free(c->box_bitvectors);

    c->row_bitvectors = NULL;
    c->col_bitvectors = NULL;
    c->box_bitvectors = NULL;
    c->side_length    = 0;
    c->block_length   = 0;

    free(c);
    *constraints = NULL;
}

bool hpp_validation_constraints_init_from_board(hpp_validation_constraints* constraints,
                                                const hpp_board*            board)
{
    if (constraints == NULL || board == NULL || board->cells == NULL)
    {
        return false;
    }

    if (board->side_length != constraints->side_length ||
        board->block_length != constraints->block_length)
    {
        return false;
    }

    // Clear all bitvectors before rebuilding state.
    memset(constraints->row_bitvectors, 0, constraints->side_length * HPP_BITVECTOR_BYTES);
    memset(constraints->col_bitvectors, 0, constraints->side_length * HPP_BITVECTOR_BYTES);
    memset(constraints->box_bitvectors,
           0,
           hpp_validation_box_count(constraints->side_length, constraints->block_length) *
               HPP_BITVECTOR_BYTES);

    // Populate bitvectors from board state.
    for (size_t row = 0; row < board->side_length; ++row)
    {
        for (size_t col = 0; col < board->side_length; ++col)
        {
            const size_t   index = (row * board->side_length) + col;
            const hpp_cell value = board->cells[index];

            if (value == BOARD_CELL_EMPTY)
            {
                continue;
            }

            if (!hpp_validation_row_add_value(constraints, row, value) ||
                !hpp_validation_col_add_value(constraints, col, value) ||
                !hpp_validation_box_add_value(constraints, row, col, value))
            {
                return false; // Board state is invalid.
            }
        }
    }

    return true;
}

bool hpp_validation_row_add_value(hpp_validation_constraints* constraints, size_t row, size_t value)
{
    // Return false if the value already exists in the row.
    hpp_bitvector_word* row_bits = hpp_row_bits_mut(constraints, row);
    if (hpp_bitvector_test(row_bits, value))
    {
        return false;
    }

    hpp_bitvector_set(row_bits, value);
    return true;
}

void hpp_validation_row_remove_value(hpp_validation_constraints* constraints,
                                     size_t                      row,
                                     size_t                      value)
{
    hpp_bitvector_clear(hpp_row_bits_mut(constraints, row), value);
}

bool hpp_validation_row_has_value(const hpp_validation_constraints* constraints,
                                  size_t                            row,
                                  size_t                            value)
{
    return hpp_bitvector_test(hpp_row_bits(constraints, row), value);
}

bool hpp_validation_col_add_value(hpp_validation_constraints* constraints, size_t col, size_t value)
{
    // Return false if the value already exists in the column.
    hpp_bitvector_word* col_bits = hpp_col_bits_mut(constraints, col);
    if (hpp_bitvector_test(col_bits, value))
    {
        return false;
    }

    hpp_bitvector_set(col_bits, value);
    return true;
}

void hpp_validation_col_remove_value(hpp_validation_constraints* constraints,
                                     size_t                      col,
                                     size_t                      value)
{
    hpp_bitvector_clear(hpp_col_bits_mut(constraints, col), value);
}

bool hpp_validation_col_has_value(const hpp_validation_constraints* constraints,
                                  size_t                            col,
                                  size_t                            value)
{
    return hpp_bitvector_test(hpp_col_bits(constraints, col), value);
}

bool hpp_validation_box_add_value(hpp_validation_constraints* constraints,
                                  size_t                      row,
                                  size_t                      col,
                                  size_t                      value)
{
    size_t              box_idx  = hpp_get_box_index(row, col, constraints->block_length);
    hpp_bitvector_word* box_bits = hpp_box_bits_mut(constraints, box_idx);

    // Return false if the value already exists in the box.
    if (hpp_bitvector_test(box_bits, value))
    {
        return false;
    }

    hpp_bitvector_set(box_bits, value);
    return true;
}

void hpp_validation_box_remove_value(hpp_validation_constraints* constraints,
                                     size_t                      row,
                                     size_t                      col,
                                     size_t                      value)
{
    size_t box_idx = hpp_get_box_index(row, col, constraints->block_length);
    hpp_bitvector_clear(hpp_box_bits_mut(constraints, box_idx), value);
}

bool hpp_validation_box_has_value(const hpp_validation_constraints* constraints,
                                  size_t                            row,
                                  size_t                            col,
                                  size_t                            value)
{
    size_t box_idx = hpp_get_box_index(row, col, constraints->block_length);
    return hpp_bitvector_test(hpp_box_bits(constraints, box_idx), value);
}

bool hpp_validation_can_place_value(const hpp_validation_constraints* constraints,
                                    size_t                            row,
                                    size_t                            col,
                                    size_t                            value)
{
    // Stop at first detected conflict.
    if (hpp_validation_row_has_value(constraints, row, value))
    {
        return false;
    }
    if (hpp_validation_col_has_value(constraints, col, value))
    {
        return false;
    }
    if (hpp_validation_box_has_value(constraints, row, col, value))
    {
        return false;
    }

    return true;
}

/* =========================================================================
 * Internal Helpers
 * ========================================================================= */

/**
 * @brief Return number of boxes on the board.
 *
 * @note Assumes classic square subgrid layout where boxes per side equals
 *       `side_length / block_length`.
 * @pre `block_length > 0` and divides `side_length`.
 * @post No state is modified.
 *
 * @param side_length Sudoku board side length.
 * @param block_length Subgrid side length.
 * @return Total number of boxes on the board.
 */
static size_t hpp_validation_box_count(size_t side_length, size_t block_length)
{
    const size_t boxes_per_side = side_length / block_length;
    return boxes_per_side * boxes_per_side;
}

/**
 * @brief Compute sub-grid index from row/column coordinates.
 *
 * @note The mapping is row-major over box coordinates.
 * @pre `block_length > 0`; `row` and `col` are in board bounds.
 * @post No state is modified.
 *
 * @param row Cell row index.
 * @param col Cell column index.
 * @param block_length Subgrid side length.
 * @return Box index corresponding to `(row, col)`.
 */
static size_t hpp_get_box_index(size_t row, size_t col, size_t block_length)
{
    return ((row / block_length) * block_length) + (col / block_length);
}

/**
 * @brief Mutable row bitvector accessor.
 *
 * @note Returns pointer into contiguous row bitvector slab.
 * @pre `constraints != NULL` and `row < constraints->side_length`.
 * @post No state is modified.
 *
 * @param constraints Validation constraint container.
 * @param row Row index to address.
 * @return Mutable pointer to row occupancy bitvector.
 */
static inline hpp_bitvector_word* hpp_row_bits_mut(hpp_validation_constraints* constraints,
                                                   size_t                      row)
{
    return constraints->row_bitvectors + (row * HPP_BITVECTOR_WORD_COUNT);
}

/**
 * @brief Const row bitvector accessor.
 *
 * @note Returns pointer into contiguous row bitvector slab.
 * @pre `constraints != NULL` and `row < constraints->side_length`.
 * @post No state is modified.
 *
 * @param constraints Validation constraint container.
 * @param row Row index to address.
 * @return Read-only pointer to row occupancy bitvector.
 */
static inline const hpp_bitvector_word* hpp_row_bits(const hpp_validation_constraints* constraints,
                                                     size_t                            row)
{
    return constraints->row_bitvectors + (row * HPP_BITVECTOR_WORD_COUNT);
}

/**
 * @brief Mutable column bitvector accessor.
 *
 * @note Returns pointer into contiguous column bitvector slab.
 * @pre `constraints != NULL` and `col < constraints->side_length`.
 * @post No state is modified.
 *
 * @param constraints Validation constraint container.
 * @param col Column index to address.
 * @return Mutable pointer to column occupancy bitvector.
 */
static inline hpp_bitvector_word* hpp_col_bits_mut(hpp_validation_constraints* constraints,
                                                   size_t                      col)
{
    return constraints->col_bitvectors + (col * HPP_BITVECTOR_WORD_COUNT);
}

/**
 * @brief Const column bitvector accessor.
 *
 * @note Returns pointer into contiguous column bitvector slab.
 * @pre `constraints != NULL` and `col < constraints->side_length`.
 * @post No state is modified.
 *
 * @param constraints Validation constraint container.
 * @param col Column index to address.
 * @return Read-only pointer to column occupancy bitvector.
 */
static inline const hpp_bitvector_word* hpp_col_bits(const hpp_validation_constraints* constraints,
                                                     size_t                            col)
{
    return constraints->col_bitvectors + (col * HPP_BITVECTOR_WORD_COUNT);
}

/**
 * @brief Mutable box bitvector accessor.
 *
 * @note Returns pointer into contiguous box bitvector slab.
 * @pre `constraints != NULL` and `box_idx < box_count`.
 * @post No state is modified.
 *
 * @param constraints Validation constraint container.
 * @param box_idx Box index to address.
 * @return Mutable pointer to box occupancy bitvector.
 */
static inline hpp_bitvector_word* hpp_box_bits_mut(hpp_validation_constraints* constraints,
                                                   size_t                      box_idx)
{
    return constraints->box_bitvectors + (box_idx * HPP_BITVECTOR_WORD_COUNT);
}

/**
 * @brief Const box bitvector accessor.
 *
 * @note Returns pointer into contiguous box bitvector slab.
 * @pre `constraints != NULL` and `box_idx < box_count`.
 * @post No state is modified.
 *
 * @param constraints Validation constraint container.
 * @param box_idx Box index to address.
 * @return Read-only pointer to box occupancy bitvector.
 */
static inline const hpp_bitvector_word* hpp_box_bits(const hpp_validation_constraints* constraints,
                                                     size_t                            box_idx)
{
    return constraints->box_bitvectors + (box_idx * HPP_BITVECTOR_WORD_COUNT);
}
