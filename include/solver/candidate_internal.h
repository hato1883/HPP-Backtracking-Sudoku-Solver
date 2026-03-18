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

enum
{
    HPP_CANDIDATE_VALUE_COUNT = HPP_BITVECTOR_VALUE_COUNT,
    HPP_CANDIDATE_WORD_COUNT  = HPP_BITVECTOR_WORD_COUNT,
    HPP_CANDIDATE_CELL_BYTES  = HPP_BITVECTOR_BYTES,
};

/**
 * @brief Test one candidate bit in a domain/constraint bitvector.
 *
 * @pre `bitvector != NULL` and `value` is in range.
 * @post Bitvector is unchanged.
 *
 * @param bitvector Target bitvector storage.
 * @param value Candidate/value index to test.
 * @return `true` when the candidate bit is set.
 */
static inline bool hpp_candidate_bit_test(const hpp_bitvector_word* bitvector, size_t value)
{
    return hpp_bitvector_test(bitvector, value);
}

/**
 * @brief Return mutable pointer to one cell's candidate domain words.
 *
 * @note Candidate domains are stored as one contiguous slab, with each cell
 *       occupying `HPP_CANDIDATE_WORD_COUNT` consecutive words.
 * @pre `state != NULL` and `cell_index < state->board->cell_count`.
 * @post Candidate state is otherwise unchanged.
 *
 * @param state Candidate state owning the domain slab.
 * @param cell_index Cell index to address.
 * @return Mutable pointer to the first domain word for the cell.
 */
static inline hpp_bitvector_word* hpp_candidate_cell_at(hpp_candidate_state* state,
                                                        size_t               cell_index)
{
    return state->candidates + (cell_index * HPP_CANDIDATE_WORD_COUNT);
}

/**
 * @brief Return read-only pointer to one cell's candidate domain words.
 *
 * @note Candidate domains are stored as one contiguous slab, with each cell
 *       occupying `HPP_CANDIDATE_WORD_COUNT` consecutive words.
 * @pre `state != NULL` and `cell_index < state->board->cell_count`.
 * @post Candidate state is unchanged.
 *
 * @param state Candidate state owning the domain slab.
 * @param cell_index Cell index to address.
 * @return Const pointer to the first domain word for the cell.
 */
static inline const hpp_bitvector_word*
hpp_candidate_cell_at_const(const hpp_candidate_state* state, size_t cell_index)
{
    return state->candidates + (cell_index * HPP_CANDIDATE_WORD_COUNT);
}

/**
 * @brief Return read-only row occupancy bitvector pointer.
 *
 * @pre `constraints != NULL` and `row < constraints->side_length`.
 * @post Constraint state is unchanged.
 *
 * @param constraints Validation constraint cache.
 * @param row Row index to address.
 * @return Const pointer to row occupancy words.
 */
static inline const hpp_bitvector_word*
hpp_candidate_row_bits(const hpp_validation_constraints* constraints, size_t row)
{
    return constraints->row_bitvectors + (row * HPP_CANDIDATE_WORD_COUNT);
}

/**
 * @brief Return read-only column occupancy bitvector pointer.
 *
 * @pre `constraints != NULL` and `col < constraints->side_length`.
 * @post Constraint state is unchanged.
 *
 * @param constraints Validation constraint cache.
 * @param col Column index to address.
 * @return Const pointer to column occupancy words.
 */
static inline const hpp_bitvector_word*
hpp_candidate_col_bits(const hpp_validation_constraints* constraints, size_t col)
{
    return constraints->col_bitvectors + (col * HPP_CANDIDATE_WORD_COUNT);
}

/**
 * @brief Return read-only box occupancy bitvector pointer.
 *
 * @pre `constraints != NULL` and `box_idx` is valid for board geometry.
 * @post Constraint state is unchanged.
 *
 * @param constraints Validation constraint cache.
 * @param box_idx Box index to address.
 * @return Const pointer to box occupancy words.
 */
static inline const hpp_bitvector_word*
hpp_candidate_box_bits(const hpp_validation_constraints* constraints, size_t box_idx)
{
    return constraints->box_bitvectors + (box_idx * HPP_CANDIDATE_WORD_COUNT);
}

/**
 * @brief Push a cell index onto the candidate propagation work stack.
 *
 * @note This helper is shared by candidate initialization and propagation,
 *       and deduplicates entries via `modified_in_stack` bookkeeping.
 * @pre `state != NULL` and `cell_index < state->board->cell_count`.
 * @post On success, `cell_index` is present in the pending-work stack.
 *
 * @param state Candidate state containing work-stack buffers.
 * @param cell_index Cell index to enqueue.
 * @return `false` on invalid state/index or capacity failure.
 */
bool hpp_candidate_modified_push(hpp_candidate_state* state, size_t cell_index);

/**
 * @brief Pop one cell index from the candidate propagation work stack.
 *
 * @note LIFO ordering keeps recently modified cells hot for follow-up checks.
 * @pre `state != NULL` and `cell_index != NULL`.
 * @post On success, one pending cell is removed and returned.
 *
 * @param state Candidate state containing work-stack buffers.
 * @param cell_index Output location for popped cell index.
 * @return `true` when a pending cell was available.
 */
bool hpp_candidate_modified_pop(hpp_candidate_state* state, size_t* cell_index);

/**
 * @brief Resolve the singleton value for a cell during AC-3 propagation.
 *
 * @note Returns board-assigned value directly when present; otherwise derives
 *       singleton from candidate domain metadata.
 * @pre `state != NULL` and `cell_index < state->board->cell_count`.
 * @post Candidate state is unchanged.
 *
 * @param state Candidate state to inspect.
 * @param cell_index Cell index to query.
 * @return Singleton value, or `BOARD_CELL_EMPTY` when unresolved.
 */
size_t hpp_candidate_ac3_singleton_value_for_cell(const hpp_candidate_state* state,
                                                  size_t                     cell_index);

#endif /* HPP_SOLVER_CANDIDATE_INTERNAL_H */
