/**
 * @file solver/candidate_internal.h
 * @brief Internal helpers shared across candidate solver translation units.
 */

#ifndef HPP_SOLVER_CANDIDATE_INTERNAL_H
#define HPP_SOLVER_CANDIDATE_INTERNAL_H

#include "data/bitvector_ops.h"
#include "solver/candidate.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum
{
    HPP_CANDIDATE_VALUE_COUNT = HPP_BITVECTOR_VALUE_COUNT,
    HPP_CANDIDATE_WORD_COUNT  = HPP_BITVECTOR_WORD_COUNT,
    HPP_CANDIDATE_CELL_BYTES  = HPP_BITVECTOR_BYTES,
};

static inline void hpp_candidate_bit_set(hpp_bitvector_word* bitvector, size_t value)
{
    hpp_bitvector_set(bitvector, value);
}

static inline void hpp_candidate_bit_clear(hpp_bitvector_word* bitvector, size_t value)
{
    hpp_bitvector_clear(bitvector, value);
}

static inline bool hpp_candidate_bit_test(const hpp_bitvector_word* bitvector, size_t value)
{
    return hpp_bitvector_test(bitvector, value);
}

static inline hpp_bitvector_word* hpp_candidate_cell_at(hpp_candidate_state* state,
                                                        size_t               cell_index)
{
    return state->candidates + (cell_index * HPP_CANDIDATE_WORD_COUNT);
}

static inline const hpp_bitvector_word*
hpp_candidate_cell_at_const(const hpp_candidate_state* state, size_t cell_index)
{
    return state->candidates + (cell_index * HPP_CANDIDATE_WORD_COUNT);
}

static inline size_t hpp_candidate_buffer_size(const hpp_board* board)
{
    return board->cell_count * HPP_CANDIDATE_CELL_BYTES;
}

static inline size_t hpp_candidate_box_index(const hpp_board* board, size_t row, size_t col)
{
    return ((row / board->block_length) * board->block_length) + (col / board->block_length);
}

static inline size_t hpp_candidate_box_count(const hpp_board* board)
{
    const size_t boxes_per_side = board->side_length / board->block_length;
    return boxes_per_side * boxes_per_side;
}

static inline const hpp_bitvector_word*
hpp_candidate_row_bits(const hpp_validation_constraints* constraints, size_t row)
{
    return constraints->row_bitvectors + (row * HPP_CANDIDATE_WORD_COUNT);
}

static inline const hpp_bitvector_word*
hpp_candidate_col_bits(const hpp_validation_constraints* constraints, size_t col)
{
    return constraints->col_bitvectors + (col * HPP_CANDIDATE_WORD_COUNT);
}

static inline const hpp_bitvector_word*
hpp_candidate_box_bits(const hpp_validation_constraints* constraints, size_t box_idx)
{
    return constraints->box_bitvectors + (box_idx * HPP_CANDIDATE_WORD_COUNT);
}

static inline const size_t* hpp_candidate_row_cells(const hpp_candidate_state* state, size_t row)
{
    return state->row_cell_order + (row * state->board->side_length);
}

static inline const size_t* hpp_candidate_col_cells(const hpp_candidate_state* state, size_t col)
{
    return state->col_cell_order + (col * state->board->side_length);
}

static inline const size_t* hpp_candidate_box_cells(const hpp_candidate_state* state,
                                                    size_t                     box_idx)
{
    return state->box_cell_order + (box_idx * state->board->side_length);
}

bool hpp_candidate_validate_initial_board(const hpp_board*            board,
                                          hpp_validation_constraints* constraints);

hpp_validation_constraints*
hpp_candidate_constraints_clone(const hpp_validation_constraints* source);

void hpp_candidate_constraints_copy(hpp_validation_constraints*       destination,
                                    const hpp_validation_constraints* source);

bool hpp_candidate_build_spatial_orders(hpp_candidate_state* state);

bool hpp_candidate_clone_spatial_orders(const hpp_candidate_state* source,
                                        hpp_candidate_state*       destination);

void hpp_candidate_destroy_spatial_orders(hpp_candidate_state* state);

bool hpp_candidate_modified_push(hpp_candidate_state* state, size_t cell_index);

bool hpp_candidate_modified_pop(hpp_candidate_state* state, size_t* cell_index);

void hpp_candidate_modified_reset(hpp_candidate_state* state);

size_t hpp_candidate_find_single_value_for_cell(const hpp_candidate_state* state,
                                                size_t                     cell_index);

size_t hpp_candidate_ac3_singleton_value_for_cell(const hpp_candidate_state* state,
                                                  size_t                     cell_index);

size_t
hpp_candidate_compute_cell(hpp_candidate_state* state, size_t cell_index, size_t* single_value);

bool hpp_candidate_initialize_cache_and_work_stack(hpp_candidate_state* state);

#endif /* HPP_SOLVER_CANDIDATE_INTERNAL_H */
