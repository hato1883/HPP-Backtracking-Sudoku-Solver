#ifndef SOLVER_SINK_H
#define SOLVER_SINK_H

#include "solver/progress.h"

#include <stdint.h>
#include <stdlib.h>

/**
 * Non-blocking progress sink callback.
 *
 * Called by the solver at configured iteration intervals (e.g., every N steps).
 * Must be very fast and non-blocking; solver thread will not wait.
 * Must not retain the progress pointer after callback returns.
 *
 * @param progress read-only progress event (transient).
 * @param userdata opaque consumer-provided context.
 * @return 0 on success; non-zero to signal abort/error (solver may ignore).
 */
typedef int (*hpp_progress_sink_callback)(const hpp_solver_progress* progress, void* userdata);

/**
 * Configuration for solver progress reporting.
 *
 * Encapsulates the callback channel and emission frequency.
 * If callback is NULL, progress is not reported (zero overhead).
 */
typedef struct ProgressSinkConfig
{
    /**
     * Callback function; NULL disables progress reporting.
     * Solver will not allocate or invoke callback if NULL.
     */
    hpp_progress_sink_callback callback;

    /** Opaque context passed to callback (e.g., TUI consumer handle). */
    void* userdata;

    /**
     * Emit progress event every N iterations.
     * E.g., progress_every_n=100 means emit after every 100 solver steps.
     * Ignored if callback is NULL.
     */
    uint32_t progress_every_n;
} hpp_progress_sink_config;

#endif
