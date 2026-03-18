/**
 * @file solver/progress.h
 * @brief Immutable payload emitted to progress sinks.
 */

#ifndef SOLVER_PROGRESS_H
#define SOLVER_PROGRESS_H

#include "solver/move.h"

#include <stdint.h>

/**
 * @brief One solver progress snapshot.
 *
 * The payload is transient and only valid for the duration of the callback
 * invocation that receives it.
 */
typedef struct SolverProgress
{
    uint64_t sequence;        /**< Monotonic event sequence number. */
    uint64_t iteration;       /**< Solver iteration counter. */
    hpp_move latest_move;     /**< Most recent move/event. */
    double   elapsed_seconds; /**< Wall-clock seconds since solve start. */
} hpp_solver_progress;

#endif /* SOLVER_PROGRESS_H */
