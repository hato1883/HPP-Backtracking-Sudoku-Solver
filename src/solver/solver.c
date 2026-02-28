#include "solver/solver.h"

#include "solver/cost.h"
#include "solver/progress.h"
#include "utils/logger.h"
#include "utils/timing.h"

#include <stddef.h>

/**
 * Internal state for the solver session.
 */
typedef struct SolverSession
{
    hpp_board*                 board;
    hpp_solver_config          config;
    hpp_progress_sink_callback progress_cb;
    void*                      progress_userdata;
    uint32_t                   progress_every_n;
    uint64_t                   iteration;
    hpp_cost                   current_cost;
    hpp_cost                   best_cost;
    hpp_timer                  timer;
} hpp_solver_session;

// TODO: implement internal helpers
static void solver_step(hpp_solver_session* session);
void        probe(void);
void        create_neighbourhood(void);

/**
 * Compute current board cost (violations/conflicts).
 * TODO: implement actual cost computation.
 */
static hpp_cost compute_cost(const hpp_board* board __attribute__((unused)))
{
    hpp_cost cost = {.violations = 0};
    // TODO: compute row, column, and block conflicts
    return cost;
}

/**
 * Emit progress to sink if configured and iteration threshold met.
 */
static int emit_progress(hpp_solver_session* session)
{
    if (session->progress_cb == NULL)
    {
        return 0;
    }
    if (session->iteration % session->progress_every_n != 0)
    {
        return 0;
    }

    hpp_solver_progress progress = {
        .sequence        = session->iteration,
        .iteration       = session->iteration,
        .cost            = session->current_cost,
        .best_cost       = session->best_cost,
        .latest_move     = {.kind = MOVE_ASSIGN}, // TODO: track actual moves
        .elapsed_seconds = timer_s(&session->timer),
    };

    int ret = session->progress_cb(&progress, session->progress_userdata);
    return ret;
}

/**
 * Main solver entry point.
 */
hpp_solver_status solve(hpp_board* board, const hpp_solver_config* config)
{
    if (board == NULL)
    {
        return SOLVER_ERROR;
    }
    const int update_rate = 100;

    hpp_solver_session session = {
        .board             = board,
        .iteration         = 0,
        .current_cost      = compute_cost(board),
        .best_cost         = {.violations = UINT32_MAX},
        .progress_cb       = NULL,
        .progress_userdata = NULL,
        .progress_every_n  = update_rate,
    };

    if (config != NULL)
    {
        session.config            = *config;
        session.progress_cb       = config->progress_sink.callback;
        session.progress_userdata = config->progress_sink.userdata;
        session.progress_every_n  = config->progress_sink.progress_every_n;
    }

    timer_start(&session.timer);

    LOG_INFO("Solver started: board %zu x %zu, max_iterations=%lu",
             board->size,
             board->size,
             session.config.max_iterations);

    // Main solver loop
    uint64_t max_iters =
        session.config.max_iterations > 0 ? session.config.max_iterations : UINT64_MAX;
    while (session.iteration < max_iters && session.current_cost.violations > 0)
    {
        solver_step(&session);

        if (emit_progress(&session) != 0)
        {
            LOG_WARN("Solver aborted by callback");
            return SOLVER_ABORTED;
        }
    }

    timer_stop(&session.timer);

    if (session.current_cost.violations == 0)
    {
        LOG_INFO("Solver converged in %lu iterations (%.4lf s)",
                 session.iteration,
                 timer_s(&session.timer));
        return SOLVER_SUCCESS;
    }

    LOG_WARN("Solver did not converge after %lu iterations (%.4lf s)",
             session.iteration,
             timer_s(&session.timer));
    return SOLVER_UNSOLVED;
}

/**
 * Take one solver iteration (step/probe/neighborhood search).
 * TODO: implement actual SLS algorithm.
 * For now, stub that just increments iteration.
 */
static void solver_step(hpp_solver_session* session)
{
    session->iteration++;
    // TODO: perform actual move/swap, update board, compute cost, emit progress
}

// TODO: implement internal helpers
// void probe(void);
// void create_neighbourhood(void);