/**
 * @file solver/solver.h
 * @brief Parallel backtracking Sudoku solver API.
 *
 * The implementation uses candidate-domain propagation plus depth-limited
 * OpenMP task parallelism. The input board is solved in-place.
 */

#ifndef SOLVER_SOLVER_H
#define SOLVER_SOLVER_H

#include "data/board.h"
#include "solver/sink.h"

#include <stdint.h>

/* =========================================================================
 * Status Codes
 * ========================================================================= */

/** Result of `solve()`. */
typedef enum SolverStatus
{
    SOLVER_SUCCESS  = 0, /**< Solved; output board is complete and consistent. */
    SOLVER_UNSOLVED = 1, /**< No solution found for the provided board. */
    SOLVER_ABORTED  = 2, /**< Reserved status for externally aborted solves. */
    SOLVER_ERROR    = 3, /**< Invalid input or internal failure. */
} hpp_solver_status;

/* =========================================================================
 * Configuration
 * ========================================================================= */

/**
 * @brief Solver configuration (all fields optional).
 *
 * @note Unused/reserved fields may be ignored by current implementation.
 */
typedef struct SolverConfig
{
    uint32_t thread_count; /**< Number of OpenMP threads (`0` = runtime default). */

    hpp_progress_sink_config progress_sink; /**< Reserved progress callback channel. */

    const char* moves_log_file; /**< Reserved move-log output path. */
} hpp_solver_config;

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Solve a board in-place.
 *
 * @note Solver uses deterministic propagation plus parallel backtracking.
 * @pre `board != NULL`, `board->cells != NULL`, and board dimensions are valid.
 * @post On `SOLVER_SUCCESS`, `board` contains a complete solution.
 * @post On `SOLVER_UNSOLVED`/`SOLVER_ERROR`, `board` may be partially modified.
 *
 * @param board  Board to solve (must be non-NULL and internally valid).
 * @param config Optional configuration, or `NULL` for defaults.
 * @return Solver status code.
 *
 * @par Example
 * @code{.c}
 * hpp_solver_config cfg = { .thread_count = 4 };
 * hpp_solver_status st = solve(board, &cfg);
 * if (st == SOLVER_SUCCESS) {
 *     print_board(board);
 * }
 * @endcode
 */
hpp_solver_status solve(hpp_board* board, const hpp_solver_config* config);

#endif /* SOLVER_SOLVER_H */