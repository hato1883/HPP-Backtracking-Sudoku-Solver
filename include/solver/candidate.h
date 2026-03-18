/**
 * @file solver/candidate.h
 * @brief Candidate-domain cache and inference helpers for backtracking.
 *
 * This layer tracks per-cell candidate domains as compact bitvectors and
 * provides deterministic propagation (AC-3 style singleton elimination plus
 * hidden-single detection) before branching.
 */

#ifndef SOLVER_CANDIDATE_H
#define SOLVER_CANDIDATE_H

#include "data/board.h"
#include "solver/validation.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* =========================================================================
 * Constants
 * ========================================================================= */

enum
{
    HPP_CANDIDATE_MAX_VALUES = UINT8_MAX,
};

/* =========================================================================
 * Data Types
 * ========================================================================= */

/**
 * @brief Full mutable state for one search node.
 */
typedef struct CandidateState
{
    hpp_board*                  board;       /**< Board snapshot for this node. */
    hpp_validation_constraints* constraints; /**< Row/col/box occupancy cache. */

    hpp_bitvector_word* candidates;       /**< Flat candidate bitvector buffer (per cell). */
    size_t*             candidate_counts; /**< Number of candidates per cell. */

    size_t* row_cell_order; /**< Row-major order: row1 c1..cN, row2 c1..cN, ... */
    size_t* col_cell_order; /**< Column-major order: c1 r1..rN, c2 r1..rN, ... */
    size_t* box_cell_order; /**< Box-major order: box local TL->BR, then next box. */

    size_t*  modified_cells;    /**< Work stack of cells to revisit. */
    uint8_t* modified_in_stack; /**< Dedup flags for `modified_cells`. */
    size_t   modified_count;    /**< Current stack size in `modified_cells`. */

    size_t remaining_unassigned; /**< Unfilled cell count. */
} hpp_candidate_state;

/** @brief Branching decision: try `values[]` at `cell_index`. */
typedef struct CandidateBranch
{
    size_t cell_index;
    size_t value_count;
    size_t values[HPP_CANDIDATE_MAX_VALUES];
} hpp_candidate_branch;

/** @brief Candidate state initialization result. */
typedef enum CandidateInitStatus
{
    HPP_CANDIDATE_INIT_OK,
    HPP_CANDIDATE_INIT_INVALID_BOARD,
    HPP_CANDIDATE_INIT_ERROR,
} hpp_candidate_init_status;

/** @brief Status returned while building a branch from a state. */
typedef enum CandidateBranchStatus
{
    HPP_CANDIDATE_BRANCH_READY,
    HPP_CANDIDATE_BRANCH_COMPLETE,
    HPP_CANDIDATE_BRANCH_CONFLICT,
} hpp_candidate_branch_status;

/* =========================================================================
 * Lifecycle and State Management
 * ========================================================================= */

/**
 * @brief Initialize a candidate state from an input board.
 *
 * @note Initializes board clone, constraints, candidate cache, and work stack.
 * @pre `state != NULL` and `source != NULL`.
 * @post On `HPP_CANDIDATE_INIT_OK`, `state` is ready for assignments/propagation.
 *
 * @param state Destination state object to initialize.
 * @param source Source board snapshot.
 * @return Candidate initialization status.
 *
 * @par Example
 * @code{.c}
 * hpp_candidate_state state = {0};
 * if (hpp_candidate_state_init_from_board(&state, board) == HPP_CANDIDATE_INIT_OK) {
 *     hpp_candidate_state_destroy(&state);
 * }
 * @endcode
 */
hpp_candidate_init_status hpp_candidate_state_init_from_board(hpp_candidate_state* state,
                                                              const hpp_board*     source);

/**
 * @brief Destroy all owned buffers in `state`.
 *
 * @pre `state != NULL`.
 * @post Owned allocations in `state` are released.
 *
 * @param state State object to destroy.
 */
void hpp_candidate_state_destroy(hpp_candidate_state* state);

/**
 * @brief Deep-clone a candidate state.
 *
 * @note Clone is independent and may diverge safely in branch search.
 * @pre `source != NULL` and `destination != NULL`.
 * @post On success, destination state matches source state.
 *
 * @param source Source state.
 * @param destination Destination state.
 * @return `true` on successful clone.
 */
bool hpp_candidate_state_clone(const hpp_candidate_state* source, hpp_candidate_state* destination);

/* =========================================================================
 * Solving Helpers
 * ========================================================================= */

/**
 * @brief Assign one value to one cell; updates constraints and caches.
 *
 * @note Assignment can fail on domain or constraint conflicts.
 * @pre `state != NULL` and `cell_index` is in range.
 * @post On success, cell is assigned and related domains are updated.
 *
 * @param state Candidate state to mutate.
 * @param cell_index Flat board cell index.
 * @param value Value to assign.
 * @return `true` if assignment succeeds.
 */
bool hpp_candidate_state_assign(hpp_candidate_state* state, size_t cell_index, size_t value);

/**
 * @brief Propagate singles until quiescence or contradiction.
 *
 * @note Applies singleton and hidden-single inference.
 * @pre `state != NULL`.
 * @post On success, no immediately derivable singles remain in work stack.
 *
 * @param state Candidate state to propagate.
 * @return `true` if no contradiction is found.
 */
bool hpp_candidate_state_propagate_singles(hpp_candidate_state* state);

/**
 * @brief Build a minimum-domain branch decision.
 *
 * @note Uses MRV-style selection (fewest candidates first).
 * @pre `state != NULL` and `branch != NULL`.
 * @post On `HPP_CANDIDATE_BRANCH_READY`, `branch` contains candidate choices.
 *
 * @param state Candidate state to branch from.
 * @param branch Output branch decision.
 * @return Branch status code.
 */
hpp_candidate_branch_status hpp_candidate_state_build_branch(hpp_candidate_state*  state,
                                                             hpp_candidate_branch* branch);

/**
 * @brief Return whether no unassigned cells remain.
 *
 * @pre `state != NULL`.
 * @post State is unchanged.
 *
 * @param state Candidate state to inspect.
 * @return `true` if assignment is complete.
 */
bool hpp_candidate_state_is_complete(const hpp_candidate_state* state);

/**
 * @brief Copy full solved-state data from `source` into `destination`.
 *
 * @pre `destination != NULL` and `source != NULL`.
 * @post Destination state equals source state.
 *
 * @param destination Destination candidate state.
 * @param source Source candidate state.
 */
void hpp_candidate_state_copy_solution(hpp_candidate_state*       destination,
                                       const hpp_candidate_state* source);

#endif /* SOLVER_CANDIDATE_H */