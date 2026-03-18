/**
 * @file solver/cactus_stack.h
 * @brief Ref-counted cactus stack for branch-local candidate states.
 *
 * A cactus stack is a parent-linked node chain where each node owns one
 * `hpp_candidate_state`. Child nodes clone parent state and diverge, enabling
 * branch exploration while sharing ancestry through reference counting.
 */

#ifndef SOLVER_CACTUS_STACK_H
#define SOLVER_CACTUS_STACK_H

#include "solver/candidate.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

/* =========================================================================
 * Node and Stack Types
 * ========================================================================= */

/** @brief One branch snapshot in the cactus stack. */
typedef struct CactusNode
{
    struct CactusNode*  parent;    /**< Parent node, or `NULL` for root. */
    _Atomic size_t      ref_count; /**< Atomic retain/release count.      */
    hpp_candidate_state state;     /**< Owned candidate state snapshot.   */
} hpp_cactus_node;

/** @brief Stack view onto a cactus-node chain. */
typedef struct CactusStack
{
    hpp_cactus_node* top;   /**< Current stack top. */
    size_t           depth; /**< Number of nodes in chain. */
} hpp_cactus_stack;

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

/**
 * @brief Initialize an empty cactus stack.
 *
 * @pre `stack != NULL`.
 * @post `stack->top == NULL` and `stack->depth == 0`.
 *
 * @param stack Stack instance to initialize.
 */
void hpp_cactus_stack_init(hpp_cactus_stack* stack);

/**
 * @brief Create a root node from an initial board.
 *
 * @note Root node starts with ref-count 1.
 * @pre `board != NULL`.
 * @post On success, returned node owns initialized candidate state.
 *
 * @param board Input board snapshot for root state creation.
 * @param status Optional out-parameter with initialization status.
 * @return Newly allocated root node, or `NULL` on failure.
 */
hpp_cactus_node* hpp_cactus_node_create_root_from_board(const hpp_board*           board,
                                                        hpp_candidate_init_status* status);

/**
 * @brief Create a child node by cloning a parent node state.
 *
 * @pre `parent != NULL`.
 * @post On success, child parent-link points to `parent`.
 *
 * @param parent Parent node to clone.
 * @return Newly allocated child node, or `NULL` on failure.
 */
hpp_cactus_node* hpp_cactus_node_create_child_clone(hpp_cactus_node* parent);

/**
 * @brief Increment node reference count.
 *
 * @pre `node != NULL`.
 * @post Node ref-count is incremented by one.
 *
 * @param node Node to retain.
 */
void hpp_cactus_node_retain(hpp_cactus_node* node);

/**
 * @brief Decrement node reference count and free when it reaches zero.
 *
 * @note Releasing a node may recursively release its ancestors.
 * @pre `node != NULL`.
 * @post Node memory is reclaimed when final reference is dropped.
 *
 * @param node Node to release.
 */
void hpp_cactus_node_release(hpp_cactus_node* node);

/**
 * @brief Destroy all nodes reachable from stack top.
 *
 * @pre `stack != NULL`.
 * @post Stack becomes empty.
 *
 * @param stack Stack to destroy.
 */
void hpp_cactus_stack_destroy(hpp_cactus_stack* stack);

/* =========================================================================
 * Stack Operations
 * ========================================================================= */

/**
 * @brief Push a new root onto an empty stack.
 *
 * @pre `stack != NULL`, `stack->top == NULL`, and `board != NULL`.
 * @post On success, stack depth becomes 1 and top points to root node.
 *
 * @param stack Target stack (must be empty).
 * @param board Input board for root initialization.
 * @return Candidate initialization status.
 *
 * @par Example
 * @code{.c}
 * hpp_cactus_stack stack;
 * hpp_cactus_stack_init(&stack);
 * if (hpp_cactus_stack_push_root_from_board(&stack, board) == HPP_CANDIDATE_INIT_OK) {
 *     hpp_cactus_stack_destroy(&stack);
 * }
 * @endcode
 */
hpp_candidate_init_status hpp_cactus_stack_push_root_from_board(hpp_cactus_stack* stack,
                                                                const hpp_board*  board);

/**
 * @brief Push a cloned child of the current top node.
 *
 * @pre `stack != NULL` and `stack->top != NULL`.
 * @post On success, depth increases by one and top points to child clone.
 *
 * @param stack Target stack.
 * @return `true` on success.
 */
bool hpp_cactus_stack_push_clone(hpp_cactus_stack* stack);

/**
 * @brief Copy top-state solution data into the parent node state.
 *
 * @pre `stack != NULL`, `stack->top != NULL`, and `stack->top->parent != NULL`.
 * @post On success, parent state equals top state.
 *
 * @param stack Stack whose top node has a parent.
 * @return `true` on success.
 */
bool hpp_cactus_stack_commit_top_to_parent(hpp_cactus_stack* stack);

/**
 * @brief Pop one node from the stack.
 *
 * @pre `stack != NULL` and stack is non-empty.
 * @post On success, depth decreases by one.
 *
 * @param stack Target stack.
 * @return `true` when a node was popped.
 */
bool hpp_cactus_stack_pop(hpp_cactus_stack* stack);

/**
 * @brief Mutable access to the current top state.
 *
 * @pre `stack != NULL`.
 * @post Stack ownership is unchanged.
 *
 * @param stack Target stack.
 * @return Pointer to top state, or `NULL` when stack is empty.
 */
hpp_candidate_state* hpp_cactus_stack_top_state(hpp_cactus_stack* stack);

/**
 * @brief Const access to the current top state.
 *
 * @pre `stack != NULL`.
 * @post Stack ownership is unchanged.
 *
 * @param stack Target stack.
 * @return Const pointer to top state, or `NULL` when stack is empty.
 */
const hpp_candidate_state* hpp_cactus_stack_top_state_const(const hpp_cactus_stack* stack);

/**
 * @brief Return current stack depth (0 for empty or NULL stack).
 *
 * @pre None.
 * @post Stack state is unchanged.
 *
 * @param stack Target stack.
 * @return Current depth.
 */
size_t hpp_cactus_stack_depth(const hpp_cactus_stack* stack);

#endif /* SOLVER_CACTUS_STACK_H */