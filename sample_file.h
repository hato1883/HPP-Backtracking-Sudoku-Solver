/**
 * @file solver/solver.h
 * @brief Core Sudoku solver: backtracking search with constraint propagation.
 *
 * Provides the main API for solving Sudoku boards using backtracking search
 * with deterministic inference. All mutable state is encapsulated in the
 * internal session struct. See solver.c for implementation details.
 *
 * The public API operates on `hpp_sudoku` (see solver/sudoku.h), which is
 * the unified container holding both cell values and per-cell candidate
 * bitmasks. The constraint layer (`hpp_constraints`) is allocated internally
 * and is not exposed through this header.
 */

#ifndef HPP_SOLVER_H
#define HPP_SOLVER_H

#include "solver/progress.h"
#include "solver/sink.h"
#include "solver/sudoku.h"

#include <stdint.h>
#include <stdio.h>

/* =========================================================================
 * Status codes
 * ====================================================================== */

/**
 * @enum hpp_solver_status
 * @brief Return value of `solve()`.
 *
 * SOLVER_SUCCESS  — board solved; all cells are filled.
 * SOLVER_UNSOLVED — no solution exists, or the puzzle is contradicted.
 * SOLVER_ABORTED  — search was terminated early by a progress callback.
 * SOLVER_ERROR    — invalid board dimensions or allocation failure.
 */
typedef enum SolverStatus
{
    SOLVER_SUCCESS  = 0,
    SOLVER_UNSOLVED = 1,
    SOLVER_ABORTED  = 2,
    SOLVER_ERROR    = 3,
} hpp_solver_status;

/* =========================================================================
 * Configuration
 * ====================================================================== */

typedef struct ProgressSinkConfig hpp_progress_sink_config;

/**
 * @brief Solver configuration (all fields optional; pass NULL for defaults).
 *
 * Controls progress reporting cadence and the optional CSV move log.
 */
typedef struct SolverConfig
{
    uint64_t                 max_iterations; /**< Max search nodes (0 = unlimited). */
    hpp_progress_sink_config progress_sink;  /**< Progress callback config.         */
    const char*              moves_log_file; /**< Path to CSV move log, or NULL.    */
} hpp_solver_config;

/* =========================================================================
 * Public API
 * ====================================================================== */

/**
 * @brief Solve a Sudoku board in-place.
 *
 * Modifies `board` in-place using backtracking search with deterministic
 * inference (naked singles and hidden singles). On success all cells are
 * filled and `board->masks` reflects the solved state. On failure the board
 * may be partially filled.
 *
 * Preconditions:
 *   - `board->block_size` must be set (done by the parser).
 *   - `board->board_size` must equal `board->block_size * board->block_size`.
 *
 * @param board   Board to solve (non-NULL, block_size must be set).
 * @param config  Solver configuration, or NULL for defaults.
 * @return `hpp_solver_status` indicating the outcome.
 */
hpp_solver_status solve(hpp_sudoku* board, const hpp_solver_config* config);

#endif /* HPP_SOLVER_H */