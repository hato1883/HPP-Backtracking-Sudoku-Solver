/**
 * @file solver/solver.c
 * @brief Parallel backtracking search using candidate propagation + OpenMP tasks.
 */

#include "solver/solver.h"

#include "data/cactus_stack.h"
#include "solver/candidate.h"

#include <omp.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

/* =========================================================================
 * Data Types
 * ========================================================================= */

typedef struct SolverShared
{
    _Atomic bool   solved;
    _Atomic bool   internal_error;
    _Atomic size_t active_tasks;
    size_t         max_active_tasks;
    size_t         task_spawn_depth;
    hpp_board*     output_board;
} hpp_solver_shared;

/* =========================================================================
 * Forward Declarations
 * ========================================================================= */

static inline bool hpp_solver_should_stop(const hpp_solver_shared* shared);
static inline void hpp_solver_set_error(hpp_solver_shared* shared);
static void        hpp_solver_publish_solution(hpp_solver_shared*         shared,
                                               const hpp_candidate_state* state);
static size_t      hpp_solver_compute_task_depth(size_t thread_count);
static bool        hpp_solver_try_reserve_task_slot(hpp_solver_shared* shared);
static void hpp_solver_search_node(hpp_cactus_node* node, hpp_solver_shared* shared, size_t depth);

/* =========================================================================
 * Public API
 * ========================================================================= */

hpp_solver_status solve(hpp_board* board, const hpp_solver_config* config)
{
    if (board == NULL || board->cells == NULL)
    {
        return SOLVER_ERROR;
    }

    hpp_candidate_init_status init_status = HPP_CANDIDATE_INIT_ERROR;
    hpp_cactus_node*          root = hpp_cactus_node_create_root_from_board(board, &init_status);
    if (root == NULL)
    {
        return (init_status == HPP_CANDIDATE_INIT_INVALID_BOARD) ? SOLVER_UNSOLVED : SOLVER_ERROR;
    }

    const size_t configured_threads =
        (config != NULL && config->thread_count > 0U) ? (size_t)config->thread_count : 0U;
    size_t thread_count = configured_threads;
    if (thread_count == 0U)
    {
        const int runtime_threads = omp_get_max_threads();
        thread_count              = (runtime_threads > 0) ? (size_t)runtime_threads : 1U;
    }

    hpp_solver_shared shared = {
        .max_active_tasks = thread_count * 8U,
        .task_spawn_depth = hpp_solver_compute_task_depth(thread_count),
        .output_board     = board,
    };
    if (shared.max_active_tasks == 0U)
    {
        shared.max_active_tasks = 1U;
    }

    atomic_init(&shared.solved, false);
    atomic_init(&shared.internal_error, false);
    atomic_init(&shared.active_tasks, 0U);

    // Keep thread count stable for predictable task scheduling.
    omp_set_dynamic(0);
    omp_set_max_active_levels(1);
    omp_set_num_threads((int)thread_count);

#pragma omp parallel num_threads((int)thread_count)
    {
#pragma omp single nowait
        {
            hpp_solver_search_node(root, &shared, 0U);
        }
    }

    const bool solved         = atomic_load_explicit(&shared.solved, memory_order_acquire);
    const bool internal_error = atomic_load_explicit(&shared.internal_error, memory_order_acquire);

    hpp_cactus_node_release(root);

    if (internal_error)
    {
        return SOLVER_ERROR;
    }

    return solved ? SOLVER_SUCCESS : SOLVER_UNSOLVED;
}

/* =========================================================================
 * Internal Helpers
 * ========================================================================= */

/**
 * @brief Check if search should stop due to solved state or internal failure.
 *
 * @note Uses acquire-loads so worker tasks observe published stop signals
 *       from other threads before continuing expensive search work.
 * @pre `shared != NULL`.
 * @post Shared state is not modified.
 *
 * @param shared Pointer to thread-shared solver state.
 * @return `true` when recursion/task expansion should terminate early.
 */
static inline bool hpp_solver_should_stop(const hpp_solver_shared* shared)
{
    return atomic_load_explicit(&shared->solved, memory_order_acquire) ||
           atomic_load_explicit(&shared->internal_error, memory_order_acquire);
}

/**
 * @brief Mark shared solver state as failed.
 *
 * @note A release-store ensures subsequent readers using acquire-loads see
 *       the failure flag and stop spawning/processing branches.
 * @pre `shared != NULL`.
 * @post `shared->internal_error` is set to `true`.
 *
 * @param shared Pointer to thread-shared solver state.
 */
static inline void hpp_solver_set_error(hpp_solver_shared* shared)
{
    atomic_store_explicit(&shared->internal_error, true, memory_order_release);
}

/**
 * @brief Publish first discovered solution into output board.
 *
 * @note Uses CAS on `shared->solved` so only one winning task copies board
 *       data; later winners skip copy to avoid races and redundant work.
 * @pre `shared != NULL` and `state != NULL`.
 * @post `output_board` is copied once when this call wins publication race.
 *
 * @param shared Shared solver state and publication target.
 * @param state Candidate state containing a solved board snapshot.
 */
static void hpp_solver_publish_solution(hpp_solver_shared* shared, const hpp_candidate_state* state)
{
    bool expected = false;
    if (atomic_compare_exchange_strong_explicit(
            &shared->solved, &expected, true, memory_order_acq_rel, memory_order_acquire))
    {
        hpp_board_copy(shared->output_board, state->board);
    }
}

/**
 * @brief Compute task spawn depth from thread count.
 *
 * @note Depth is capped to avoid runaway task fan-out while still creating
 *       enough parallel slack for high-core machines.
 * @pre None.
 * @post No global state is modified.
 *
 * @param thread_count Effective OpenMP worker count.
 * @return Maximum recursion depth eligible for task spawning.
 */
static size_t hpp_solver_compute_task_depth(size_t thread_count)
{
    size_t depth    = 0;
    size_t capacity = 1;
    size_t target   = (thread_count < 2U) ? 1U : (thread_count * 4U);

    while (capacity < target && depth < 10U)
    {
        capacity <<= 1U;
        depth++;
    }

    return depth;
}

/**
 * @brief Try to reserve one slot in global task budget.
 *
 * @note Uses CAS retry loop on `active_tasks` to throttle task creation and
 *       keep memory/runtime overhead bounded under heavy branching.
 * @pre `shared != NULL`.
 * @post On success, `active_tasks` is incremented by one.
 *
 * @param shared Shared solver state with budget counters.
 * @return `true` when a task slot was reserved.
 */
static bool hpp_solver_try_reserve_task_slot(hpp_solver_shared* shared)
{
    size_t active = atomic_load_explicit(&shared->active_tasks, memory_order_relaxed);
    while (active < shared->max_active_tasks)
    {
        if (atomic_compare_exchange_weak_explicit(&shared->active_tasks,
                                                  &active,
                                                  active + 1U,
                                                  memory_order_acq_rel,
                                                  memory_order_relaxed))
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief Recursive DFS node evaluation and branching.
 *
 * @note Each call performs: propagate, terminal checks, MRV branch build,
 *       then optional task-spawned child exploration under budget/depth caps.
 * @pre `shared != NULL`; `node` may be NULL (treated as no-op).
 * @post May publish a solution, mark internal error, or return without change.
 *
 * @param node Current cactus node to evaluate.
 * @param shared Shared solver control/publication state.
 * @param depth Current recursion depth used for spawn policy decisions.
 */
static void hpp_solver_search_node(hpp_cactus_node* node, hpp_solver_shared* shared, size_t depth)
{
    if (node == NULL || hpp_solver_should_stop(shared))
    {
        return;
    }

    hpp_candidate_state* state = &node->state;
    if (!hpp_candidate_state_propagate_singles(state))
    {
        return;
    }

    if (hpp_candidate_state_is_complete(state))
    {
        hpp_solver_publish_solution(shared, state);
        return;
    }

    hpp_candidate_branch              branch = {0};
    const hpp_candidate_branch_status branch_status =
        hpp_candidate_state_build_branch(state, &branch);

    if (branch_status == HPP_CANDIDATE_BRANCH_COMPLETE)
    {
        hpp_solver_publish_solution(shared, state);
        return;
    }

    if (branch_status == HPP_CANDIDATE_BRANCH_CONFLICT || branch.value_count == 0)
    {
        return;
    }

#pragma omp taskgroup
    {
        for (size_t idx = 0; idx < branch.value_count; ++idx)
        {
            if (hpp_solver_should_stop(shared))
            {
                break;
            }

            const size_t guess      = branch.values[idx];
            const size_t cell_index = branch.cell_index;

            // Spawn only while shallow enough and global task budget allows it.
            const bool can_spawn_task = (depth < shared->task_spawn_depth) &&
                                        (branch.value_count > 1U) &&
                                        hpp_solver_try_reserve_task_slot(shared);

#pragma omp task default(none)                                                                     \
    firstprivate(node, shared, depth, can_spawn_task, cell_index, guess) if (can_spawn_task)
            {
                if (!hpp_solver_should_stop(shared))
                {
                    hpp_cactus_node* child = hpp_cactus_node_create_child_clone(node);
                    if (child == NULL)
                    {
                        hpp_solver_set_error(shared);
                    }
                    else
                    {
                        if (hpp_candidate_state_assign(&child->state, cell_index, guess))
                        {
                            hpp_solver_search_node(child, shared, depth + 1U);
                        }

                        hpp_cactus_node_release(child);
                    }
                }

                if (can_spawn_task)
                {
                    (void)atomic_fetch_sub_explicit(
                        &shared->active_tasks, 1U, memory_order_acq_rel);
                }
            }
        }
    }
}
