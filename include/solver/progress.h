#ifndef SOLVER_PROGRESS_H
#define SOLVER_PROGRESS_H

#include "solver/move.h"

#include <stdint.h>

/**
 * Immutable snapshot of solver progress at a given iteration.
 *
 * This is the event payload emitted by the solver to progress sinks.
 * All fields are read-only; consumers must not modify.
 * Lifetime: valid only during callback execution; must not be retained by pointer.
 */
typedef struct SolverProgress
{
    /** Unique event sequence number (monotonic). */
    uint64_t sequence;

    /** Iteration counter in the solver. */
    uint64_t iteration;

    /** Latest move that was applied (or invalid if no move yet). */
    hpp_move latest_move;

    /** Elapsed time in seconds since solver start. */
    double elapsed_seconds;
} hpp_solver_progress;

#endif
