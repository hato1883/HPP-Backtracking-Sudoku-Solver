#ifndef SOLVER_SOLVER_H
#define SOLVER_SOLVER_H

#include "solver/sink.h"
#include "data/board.h"

#include <stdint.h>

/**
 * Solver result status.
 */
typedef enum SolverStatus
{
    SOLVER_SUCCESS  = 0, // Solved without conflicts
    SOLVER_UNSOLVED = 1, // Did not converge
    SOLVER_ABORTED  = 2, // Aborted by callback return
    SOLVER_ERROR    = 3, // Internal error
} hpp_solver_status;

/**
 * Solver configuration.
 *
 * Controls solver behavior and progress reporting.
 */
typedef struct SolverConfig
{
    /** Max iterations before giving up; 0 = unlimited. */
    uint64_t max_iterations;

    /** Progress reporting sink (NULL = no reporting, zero overhead). */
    hpp_progress_sink_config progress_sink;

    /** File path to log all moves; NULL = no logging. Useful for cycle detection. */
    const char* moves_log_file;
} hpp_solver_config;

/**
 * Solve a Sudoku board using Stochastic Local Search (SLS).
 *
 * The board is modified in-place. Progress is reported via the configured sink
 * (if non-NULL) at iteration intervals. The solver may be interrupted if a
 * progress callback returns non-zero.
 *
 * @param board the board to solve (modified in-place).
 * @param config solver configuration (may be NULL for defaults).
 * @return solver status code.
 */
hpp_solver_status solve(hpp_board* board, const hpp_solver_config* config);

#endif