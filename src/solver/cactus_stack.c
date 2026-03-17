#include "solver/cactus_stack.h"

#include <stdlib.h>

static hpp_cactus_node* hpp_cactus_node_create(void)
{
    hpp_cactus_node* node = calloc(1, sizeof(hpp_cactus_node));
    if (node != NULL)
    {
        atomic_init(&node->ref_count, 1U);
    }

    return node;
}

static void hpp_cactus_node_destroy(hpp_cactus_node* node)
{
    hpp_candidate_state_destroy(&node->state);
    free(node);
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

hpp_cactus_node* hpp_cactus_node_create_root_from_board(const hpp_board*           board,
                                                        hpp_candidate_init_status* status)
{
    if (status != NULL)
    {
        *status = HPP_CANDIDATE_INIT_ERROR;
    }

    hpp_cactus_node* node = hpp_cactus_node_create();
    if (node == NULL)
    {
        return NULL;
    }

    const hpp_candidate_init_status init_status =
        hpp_candidate_state_init_from_board(&node->state, board);
    if (init_status != HPP_CANDIDATE_INIT_OK)
    {
        if (status != NULL)
        {
            *status = init_status;
        }

        hpp_cactus_node_release(node);
        return NULL;
    }

    if (status != NULL)
    {
        *status = HPP_CANDIDATE_INIT_OK;
    }

    return node;
}

hpp_cactus_node* hpp_cactus_node_create_child_clone(hpp_cactus_node* parent)
{
    if (parent == NULL)
    {
        return NULL;
    }

    hpp_cactus_node* node = hpp_cactus_node_create();
    if (node == NULL)
    {
        return NULL;
    }

    if (!hpp_candidate_state_clone(&parent->state, &node->state))
    {
        hpp_cactus_node_release(node);
        return NULL;
    }

    hpp_cactus_node_retain(parent);
    node->parent = parent;
    return node;
}

void hpp_cactus_node_retain(hpp_cactus_node* node)
{
    if (node == NULL)
    {
        return;
    }

    (void)atomic_fetch_add_explicit(&node->ref_count, 1U, memory_order_relaxed);
}

void hpp_cactus_node_release(hpp_cactus_node* node)
{
    while (node != NULL)
    {
        if (atomic_fetch_sub_explicit(&node->ref_count, 1U, memory_order_acq_rel) != 1U)
        {
            return;
        }

        hpp_cactus_node* parent = node->parent;
        hpp_cactus_node_destroy(node);
        node = parent;
    }
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

    hpp_candidate_init_status init_status = HPP_CANDIDATE_INIT_ERROR;
    hpp_cactus_node*          node = hpp_cactus_node_create_root_from_board(board, &init_status);
    if (node == NULL)
    {
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

    hpp_cactus_node* parent = stack->top;
    hpp_cactus_node* child  = hpp_cactus_node_create_child_clone(parent);
    if (child == NULL)
    {
        return false;
    }

    stack->top = child;
    stack->depth++;

    // Ownership of the old top node moves from the stack to the new child.
    hpp_cactus_node_release(parent);
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
    if (node->parent != NULL)
    {
        // The stack takes ownership of the parent before releasing the child.
        hpp_cactus_node_retain(node->parent);
    }

    stack->top = node->parent;
    hpp_cactus_node_release(node);

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
