#include "solver/cactus_stack.h"

#include <stdlib.h>

static hpp_cactus_node* hpp_cactus_node_create(void)
{
    return calloc(1, sizeof(hpp_cactus_node));
}

void hpp_cactus_stack_init(hpp_cactus_stack* stack)
{
    if (stack == NULL)
    {
        return;
    }

    stack->top   = NULL;
    stack->depth = 0;
}

void hpp_cactus_stack_destroy(hpp_cactus_stack* stack)
{
    if (stack == NULL)
    {
        return;
    }

    while (stack->top != NULL)
    {
        (void)hpp_cactus_stack_pop(stack);
    }
}

hpp_candidate_init_status hpp_cactus_stack_push_root_from_board(hpp_cactus_stack* stack,
                                                                const hpp_board*  board)
{
    if (stack == NULL || stack->top != NULL)
    {
        return HPP_CANDIDATE_INIT_ERROR;
    }

    hpp_cactus_node* node = hpp_cactus_node_create();
    if (node == NULL)
    {
        return HPP_CANDIDATE_INIT_ERROR;
    }

    const hpp_candidate_init_status init_status =
        hpp_candidate_state_init_from_board(&node->state, board);
    if (init_status != HPP_CANDIDATE_INIT_OK)
    {
        hpp_candidate_state_destroy(&node->state);
        free(node);
        return init_status;
    }

    stack->top = node;
    stack->depth++;
    return HPP_CANDIDATE_INIT_OK;
}

bool hpp_cactus_stack_push_clone(hpp_cactus_stack* stack)
{
    if (stack == NULL || stack->top == NULL)
    {
        return false;
    }

    hpp_cactus_node* node = hpp_cactus_node_create();
    if (node == NULL)
    {
        return false;
    }

    if (!hpp_candidate_state_clone(&stack->top->state, &node->state))
    {
        hpp_candidate_state_destroy(&node->state);
        free(node);
        return false;
    }

    node->parent = stack->top;
    stack->top   = node;
    stack->depth++;
    return true;
}

bool hpp_cactus_stack_commit_top_to_parent(hpp_cactus_stack* stack)
{
    if (stack == NULL || stack->top == NULL || stack->top->parent == NULL)
    {
        return false;
    }

    hpp_candidate_state_copy_solution(&stack->top->parent->state, &stack->top->state);
    return true;
}

bool hpp_cactus_stack_pop(hpp_cactus_stack* stack)
{
    if (stack == NULL || stack->top == NULL)
    {
        return false;
    }

    hpp_cactus_node* node = stack->top;
    stack->top            = node->parent;

    hpp_candidate_state_destroy(&node->state);
    free(node);

    if (stack->depth > 0)
    {
        stack->depth--;
    }

    return true;
}

hpp_candidate_state* hpp_cactus_stack_top_state(hpp_cactus_stack* stack)
{
    if (stack == NULL || stack->top == NULL)
    {
        return NULL;
    }

    return &stack->top->state;
}

const hpp_candidate_state* hpp_cactus_stack_top_state_const(const hpp_cactus_stack* stack)
{
    if (stack == NULL || stack->top == NULL)
    {
        return NULL;
    }

    return &stack->top->state;
}

size_t hpp_cactus_stack_depth(const hpp_cactus_stack* stack)
{
    return (stack == NULL) ? 0 : stack->depth;
}
