#ifndef SOLVER_CACTUS_STACK_H
#define SOLVER_CACTUS_STACK_H

#include "solver/candidate.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct CactusNode
{
    struct CactusNode*  parent;
    _Atomic size_t      ref_count;
    hpp_candidate_state state;
} hpp_cactus_node;

typedef struct CactusStack
{
    hpp_cactus_node* top;
    size_t           depth;
} hpp_cactus_stack;

void hpp_cactus_stack_init(hpp_cactus_stack* stack);

hpp_cactus_node* hpp_cactus_node_create_root_from_board(const hpp_board*           board,
                                                        hpp_candidate_init_status* status);

hpp_cactus_node* hpp_cactus_node_create_child_clone(hpp_cactus_node* parent);

void hpp_cactus_node_retain(hpp_cactus_node* node);

void hpp_cactus_node_release(hpp_cactus_node* node);

void hpp_cactus_stack_destroy(hpp_cactus_stack* stack);

hpp_candidate_init_status hpp_cactus_stack_push_root_from_board(hpp_cactus_stack* stack,
                                                                const hpp_board*  board);

bool hpp_cactus_stack_push_clone(hpp_cactus_stack* stack);

bool hpp_cactus_stack_commit_top_to_parent(hpp_cactus_stack* stack);

bool hpp_cactus_stack_pop(hpp_cactus_stack* stack);

hpp_candidate_state* hpp_cactus_stack_top_state(hpp_cactus_stack* stack);

const hpp_candidate_state* hpp_cactus_stack_top_state_const(const hpp_cactus_stack* stack);

size_t hpp_cactus_stack_depth(const hpp_cactus_stack* stack);

#endif