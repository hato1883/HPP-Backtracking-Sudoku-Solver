/**
 * @file data/validation.c
 * @brief Bitvector-backed row/column/box constraint operations.
 */

#include "data/validation.h"

#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Internal Constants and Helpers
 * ========================================================================= */

enum
{
    HPP_VALIDATION_VALUE_COUNT = UINT8_MAX + 1U,
    HPP_VALIDATION_BYTE_COUNT  = HPP_VALIDATION_VALUE_COUNT / CHAR_BIT,
};

// Convert a value index into (byte, bit) coordinates.
static void hpp_bit_position(size_t value, size_t* byte_idx, uint8_t* bit_offset)
{
    *byte_idx   = value / CHAR_BIT;
    *bit_offset = (uint8_t)(value % CHAR_BIT);
}

// Compute sub-grid index from row/column coordinates.
static size_t hpp_get_box_index(size_t row, size_t col, size_t block_length)
{
    return ((row / block_length) * block_length) + (col / block_length);
}

hpp_validation_constraints* hpp_validation_constraints_create(size_t side_length,
                                                              size_t block_length)
{
    hpp_validation_constraints* constraints = malloc(sizeof(hpp_validation_constraints));
    if (constraints == NULL)
    {
        return NULL;
    }

    constraints->side_length  = side_length;
    constraints->block_length = block_length;

    // Allocate row bitvectors
    constraints->row_bitvectors = malloc(side_length * sizeof(uint8_t*));
    if (constraints->row_bitvectors == NULL)
    {
        free(constraints);
        return NULL;
    }

    for (size_t i = 0; i < side_length; ++i)
    {
        constraints->row_bitvectors[i] = calloc(BYTES_PER_BITVECTOR, sizeof(uint8_t));
        if (constraints->row_bitvectors[i] == NULL)
        {
            for (size_t j = 0; j < i; ++j)
            {
                free(constraints->row_bitvectors[j]);
            }
            free(constraints->row_bitvectors);
            free(constraints);
            return NULL;
        }
    }

    // Allocate column bitvectors
    constraints->col_bitvectors = malloc(side_length * sizeof(uint8_t*));
    if (constraints->col_bitvectors == NULL)
    {
        for (size_t i = 0; i < side_length; ++i)
        {
            free(constraints->row_bitvectors[i]);
        }
        free(constraints->row_bitvectors);
        free(constraints);
        return NULL;
    }

    for (size_t i = 0; i < side_length; ++i)
    {
        constraints->col_bitvectors[i] = calloc(BYTES_PER_BITVECTOR, sizeof(uint8_t));
        if (constraints->col_bitvectors[i] == NULL)
        {
            for (size_t j = 0; j < i; ++j)
            {
                free(constraints->col_bitvectors[j]);
            }
            free(constraints->col_bitvectors);
            for (size_t j = 0; j < side_length; ++j)
            {
                free(constraints->row_bitvectors[j]);
            }
            free(constraints->row_bitvectors);
            free(constraints);
            return NULL;
        }
    }

    // Allocate box bitvectors
    size_t num_boxes            = (side_length / block_length) * (side_length / block_length);
    constraints->box_bitvectors = malloc(num_boxes * sizeof(uint8_t*));
    if (constraints->box_bitvectors == NULL)
    {
        for (size_t i = 0; i < side_length; ++i)
        {
            free(constraints->row_bitvectors[i]);
            free(constraints->col_bitvectors[i]);
        }
        free(constraints->row_bitvectors);
        free(constraints->col_bitvectors);
        free(constraints);
        return NULL;
    }

    for (size_t i = 0; i < num_boxes; ++i)
    {
        constraints->box_bitvectors[i] = calloc(BYTES_PER_BITVECTOR, sizeof(uint8_t));
        if (constraints->box_bitvectors[i] == NULL)
        {
            for (size_t j = 0; j < i; ++j)
            {
                free(constraints->box_bitvectors[j]);
            }
            free(constraints->box_bitvectors);
            for (size_t j = 0; j < side_length; ++j)
            {
                free(constraints->row_bitvectors[j]);
                free(constraints->col_bitvectors[j]);
            }
            free(constraints->row_bitvectors);
            free(constraints->col_bitvectors);
            free(constraints);
            return NULL;
        }
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

    if (c->row_bitvectors != NULL)
    {
        for (size_t i = 0; i < c->side_length; ++i)
        {
            free(c->row_bitvectors[i]);
        }
        free(c->row_bitvectors);
    }

    if (c->col_bitvectors != NULL)
    {
        for (size_t i = 0; i < c->side_length; ++i)
        {
            free(c->col_bitvectors[i]);
        }
        free(c->col_bitvectors);
    }

    if (c->box_bitvectors != NULL)
    {
        size_t num_boxes = (c->side_length / c->block_length) * (c->side_length / c->block_length);
        for (size_t i = 0; i < num_boxes; ++i)
        {
            free(c->box_bitvectors[i]);
        }
        free(c->box_bitvectors);
    }

    free(c);
    *constraints = NULL;
}

bool hpp_validation_constraints_init_from_board(hpp_validation_constraints* constraints,
                                                const hpp_board*            board)
{
    if (constraints == NULL || board == NULL)
    {
        return false;
    }

    // Clear all bitvectors
    for (size_t i = 0; i < constraints->side_length; ++i)
    {
        memset(constraints->row_bitvectors[i], 0, BYTES_PER_BITVECTOR);
        memset(constraints->col_bitvectors[i], 0, BYTES_PER_BITVECTOR);
    }

    size_t num_boxes = (constraints->side_length / constraints->block_length) *
                       (constraints->side_length / constraints->block_length);
    for (size_t i = 0; i < num_boxes; ++i)
    {
        memset(constraints->box_bitvectors[i], 0, BYTES_PER_BITVECTOR);
    }

    // Populate bitvectors from board state
    for (size_t row = 0; row < board->side_length; ++row)
    {
        for (size_t col = 0; col < board->side_length; ++col)
        {
            size_t   index = (row * board->side_length) + col;
            hpp_cell value = board->cells[index];

            if (value != BOARD_CELL_EMPTY)
            {
                if (!hpp_validation_row_add_value(constraints, row, value) ||
                    !hpp_validation_col_add_value(constraints, col, value) ||
                    !hpp_validation_box_add_value(constraints, row, col, value))
                {
                    return false; // Board state is invalid
                }
            }
        }
    }

    return true;
}

// Set one value bit in a bitvector.
static inline void hpp_set_bit(uint8_t* bitvector, size_t value)
{
    size_t  byte_idx   = 0;
    uint8_t bit_offset = 0;
    hpp_bit_position(value, &byte_idx, &bit_offset);
    bitvector[byte_idx] |= (1U << bit_offset);
}

// Clear one value bit in a bitvector.
static inline void hpp_clear_bit(uint8_t* bitvector, size_t value)
{
    size_t  byte_idx   = 0;
    uint8_t bit_offset = 0;
    hpp_bit_position(value, &byte_idx, &bit_offset);
    bitvector[byte_idx] &= ~(1U << bit_offset);
}

// Test one value bit in a bitvector.
static inline bool hpp_test_bit(const uint8_t* bitvector, size_t value)
{
    size_t  byte_idx   = 0;
    uint8_t bit_offset = 0;
    hpp_bit_position(value, &byte_idx, &bit_offset);
    return (bitvector[byte_idx] & (1U << bit_offset)) != 0;
}

bool hpp_validation_row_add_value(hpp_validation_constraints* constraints, size_t row, size_t value)
{
    // Early termination: return false if value already exists
    if (hpp_test_bit(constraints->row_bitvectors[row], value))
    {
        return false;
    }
    hpp_set_bit(constraints->row_bitvectors[row], value);
    return true;
}

void hpp_validation_row_remove_value(hpp_validation_constraints* constraints,
                                     size_t                      row,
                                     size_t                      value)
{
    hpp_clear_bit(constraints->row_bitvectors[row], value);
}

bool hpp_validation_row_has_value(const hpp_validation_constraints* constraints,
                                  size_t                            row,
                                  size_t                            value)
{
    return hpp_test_bit(constraints->row_bitvectors[row], value);
}

bool hpp_validation_col_add_value(hpp_validation_constraints* constraints, size_t col, size_t value)
{
    // Early termination: return false if value already exists
    if (hpp_test_bit(constraints->col_bitvectors[col], value))
    {
        return false;
    }
    hpp_set_bit(constraints->col_bitvectors[col], value);
    return true;
}

void hpp_validation_col_remove_value(hpp_validation_constraints* constraints,
                                     size_t                      col,
                                     size_t                      value)
{
    hpp_clear_bit(constraints->col_bitvectors[col], value);
}

bool hpp_validation_col_has_value(const hpp_validation_constraints* constraints,
                                  size_t                            col,
                                  size_t                            value)
{
    return hpp_test_bit(constraints->col_bitvectors[col], value);
}

bool hpp_validation_box_add_value(hpp_validation_constraints* constraints,
                                  size_t                      row,
                                  size_t                      col,
                                  size_t                      value)
{
    size_t box_idx = hpp_get_box_index(row, col, constraints->block_length);

    // Early termination: return false if value already exists
    if (hpp_test_bit(constraints->box_bitvectors[box_idx], value))
    {
        return false;
    }
    hpp_set_bit(constraints->box_bitvectors[box_idx], value);
    return true;
}

void hpp_validation_box_remove_value(hpp_validation_constraints* constraints,
                                     size_t                      row,
                                     size_t                      col,
                                     size_t                      value)
{
    size_t box_idx = hpp_get_box_index(row, col, constraints->block_length);
    hpp_clear_bit(constraints->box_bitvectors[box_idx], value);
}

bool hpp_validation_box_has_value(const hpp_validation_constraints* constraints,
                                  size_t                            row,
                                  size_t                            col,
                                  size_t                            value)
{
    size_t box_idx = hpp_get_box_index(row, col, constraints->block_length);
    return hpp_test_bit(constraints->box_bitvectors[box_idx], value);
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
