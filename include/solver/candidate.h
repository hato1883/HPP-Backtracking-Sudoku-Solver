#ifndef SOLVER_CANDIDATE_H
#define SOLVER_CANDIDATE_H

#include "data/board.h"
#include "data/validation.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    HPP_CANDIDATE_MAX_VALUES = UINT8_MAX,
};

typedef struct CandidateState
{
    hpp_board*                  board;
    hpp_validation_constraints* constraints;
    uint8_t*                    candidates;
    size_t*                     candidate_counts;
    size_t*                     modified_cells;
    uint8_t*                    modified_in_stack;
    size_t                      modified_count;
    size_t                      remaining_unassigned;
} hpp_candidate_state;

typedef struct CandidateBranch
{
    size_t cell_index;
    size_t value_count;
    size_t values[HPP_CANDIDATE_MAX_VALUES];
} hpp_candidate_branch;

typedef enum CandidateInitStatus
{
    HPP_CANDIDATE_INIT_OK,
    HPP_CANDIDATE_INIT_INVALID_BOARD,
    HPP_CANDIDATE_INIT_ERROR,
} hpp_candidate_init_status;

typedef enum CandidateBranchStatus
{
    HPP_CANDIDATE_BRANCH_READY,
    HPP_CANDIDATE_BRANCH_COMPLETE,
    HPP_CANDIDATE_BRANCH_CONFLICT,
} hpp_candidate_branch_status;

hpp_candidate_init_status hpp_candidate_state_init_from_board(hpp_candidate_state* state,
                                                              const hpp_board*     source);

void hpp_candidate_state_destroy(hpp_candidate_state* state);

bool hpp_candidate_state_clone(const hpp_candidate_state* source, hpp_candidate_state* destination);

bool hpp_candidate_state_assign(hpp_candidate_state* state, size_t cell_index, size_t value);

bool hpp_candidate_state_propagate_singles(hpp_candidate_state* state);

hpp_candidate_branch_status hpp_candidate_state_build_branch(hpp_candidate_state*  state,
                                                             hpp_candidate_branch* branch);

bool hpp_candidate_state_is_complete(const hpp_candidate_state* state);

void hpp_candidate_state_copy_solution(hpp_candidate_state*       destination,
                                       const hpp_candidate_state* source);

#endif