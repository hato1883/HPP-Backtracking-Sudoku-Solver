/**
 * @file solver/sink.h
 * @brief Progress callback channel configuration.
 */

#ifndef SOLVER_SINK_H
#define SOLVER_SINK_H

#include "solver/progress.h"

#include <stdint.h>

/**
 * @brief Progress callback signature.
 *
 * Implementations should avoid blocking work. `progress` is read-only and
 * transient; copy if persistence is required.
 *
 * @note Callback may execute at high frequency depending on solver config.
 * @pre `progress != NULL` during invocation.
 * @post Solver state is unaffected by callback side effects unless callback
 * requests abort via non-zero return value.
 *
 * @param progress Read-only progress snapshot.
 * @param userdata Opaque user context pointer.
 * @return `0` to continue, non-zero to request abort.
 *
 * @par Example
 * @code{.c}
 * static int on_progress(const hpp_solver_progress* p, void* userdata) {
 *     (void)userdata;
 *     return (p->iteration > 1000000U) ? 1 : 0;
 * }
 * @endcode
 */
typedef int (*hpp_progress_sink_callback)(const hpp_solver_progress* progress, void* userdata);

/**
 * @brief Callback channel configuration for progress events.
 */
typedef struct ProgressSinkConfig
{
    hpp_progress_sink_callback callback;         /**< Callback entrypoint (`NULL` disables). */
    void*                      userdata;         /**< Opaque callback context pointer. */
    uint32_t                   progress_every_n; /**< Emit every N iterations. */
} hpp_progress_sink_config;

#endif /* SOLVER_SINK_H */
