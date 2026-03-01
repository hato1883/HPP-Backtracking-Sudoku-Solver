#include "solver/solver.h"

#include "solver/cost.h"
#include "solver/incremental_cost.h"
#include "solver/progress.h"
#include "solver/sudoku.h"
#include "utils/logger.h"
#include "utils/timing.h"

#include <limits.h>
#include <omp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/**
 * Tabu list entry: tracks a tabu move and when its tenure expires.
 */
typedef struct TabuEntry
{
    hpp_move move;
    uint64_t expiration_iteration;
} hpp_tabu_entry;

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
    hpp_move                   latest_move;

    // Tabu list for local search
    hpp_tabu_entry* tabu_list;
    size_t          tabu_count;
    size_t          tabu_capacity;
    uint32_t        tabu_tenure;

    // Incremental cost table for fast delta computation
    hpp_incremental_cost* incremental_cost;

    // Moves log file (for cycle detection)
    FILE* moves_log;

    // Random restart tracking
    uint64_t  last_improvement_iteration;
    uint64_t  restart_threshold; // Iterations without improvement before restart
    uint32_t  restart_count;
    uint8_t** best_board_state; // Save best board configuration found
} hpp_solver_session;

typedef enum ProbeStrategy
{
    PROBE_STRATEGY_BEST_IMPROVING,
    PROBE_STRATEGY_FIRST_IMPROVING,
} hpp_probe_strategy;

/**
 * Forward declarations of all private helper functions.
 */

// Random restart helpers
static uint8_t** allocate_board_state(size_t size);
static void      free_board_state(uint8_t** state, size_t size);
static void      save_board_state(uint8_t** dest, hpp_board* src);
static void      restore_board_state(hpp_board* dest, uint8_t** src);
static void      perform_restart(hpp_solver_session* session);

// Move array capacity management
static int ensure_move_capacity(hpp_move** moves, size_t* capacity, size_t move_count);

// Move creation helpers
static int append_assign_move(hpp_move** moves,
                              size_t*    move_count,
                              size_t*    capacity,
                              uint16_t   row,
                              uint16_t   col,
                              uint8_t    old_value,
                              uint8_t    new_value);
static int append_swap_move(hpp_move** moves,
                            size_t*    move_count,
                            size_t*    capacity,
                            uint16_t   row_source,
                            uint16_t   col_source,
                            uint16_t   row_destination,
                            uint16_t   col_destination);

// Move generation
static int       generate_assign_moves(const hpp_board* board,
                                       hpp_move**       moves,
                                       size_t*          move_count,
                                       size_t*          capacity);
static int       generate_row_swap_moves(const hpp_board* board,
                                         hpp_move**       moves,
                                         size_t*          move_count,
                                         size_t*          capacity);
static int       generate_block_swaps_from_cell(const hpp_board* board,
                                                hpp_move**       moves,
                                                size_t*          move_count,
                                                size_t*          capacity,
                                                size_t           start_row,
                                                size_t           start_col,
                                                size_t           source_sub_row,
                                                size_t           source_sub_col);
static int       generate_block_swap_moves(const hpp_board* board,
                                           hpp_move**       moves,
                                           size_t*          move_count,
                                           size_t*          capacity);
static hpp_move* create_neighbourhood(const hpp_board* board, size_t* out_count);

// Board mutations
static void apply_move(hpp_board* board, const hpp_move* move);
static void undo_move(hpp_board* board, const hpp_move* move);

// Cost evaluation
static hpp_cost   compute_cost(const hpp_board* board);
static inline int cost_compare(const hpp_cost* cost1, const hpp_cost* cost2);

// Neighbourhood probing strategies
static int probe_best_improving(hpp_solver_session* session,
                                const hpp_move*     moves,
                                size_t              move_count,
                                hpp_move*           best_move,
                                hpp_cost*           best_cost);
static int probe_first_improving(hpp_solver_session* session,
                                 const hpp_move*     moves,
                                 size_t              move_count,
                                 hpp_move*           best_move,
                                 hpp_cost*           best_cost);
static int probe_neighbourhood(hpp_solver_session* session,
                               const hpp_move*     moves,
                               size_t              move_count,
                               hpp_probe_strategy  strategy,
                               hpp_move*           best_move,
                               hpp_cost*           best_cost);

// Tabu list management
static int  is_tabu(const hpp_solver_session* session, const hpp_move* move);
static int  add_tabu(hpp_solver_session* session, const hpp_move* move);
static void prune_tabu(hpp_solver_session* session);

// Move logging
static void log_move(hpp_solver_session* session, const hpp_move* move);

// Progress reporting
static int emit_progress(hpp_solver_session* session);

// Solver iteration
static void solver_step(hpp_solver_session* session);

/**
 * Main solver entry point.
 */
hpp_solver_status solve(hpp_board* board, const hpp_solver_config* config)
{
    if (board == NULL)
    {
        return SOLVER_ERROR;
    }

    hpp_cost     initial_cost  = compute_cost(board);
    const int    update_rate   = 10;
    const size_t tabu_capacity = 256; // Max tabu list size

    hpp_solver_session session = {
        .board             = board,
        .iteration         = 0,
        .current_cost      = initial_cost,
        .best_cost         = initial_cost,
        .progress_cb       = NULL,
        .progress_userdata = NULL,
        .progress_every_n  = update_rate,
        .latest_move       = {.kind = MOVE_ASSIGN},
        .tabu_list         = NULL,
        .tabu_count        = 0,
        .tabu_capacity     = tabu_capacity,
        .tabu_tenure       = (uint32_t)(board->size * 2), // Increased tenure to prevent cycling
        .incremental_cost  = NULL,
        .moves_log         = NULL,
        .last_improvement_iteration = 0,
        .restart_threshold          = (uint64_t)(board->size * board->size *
                                        10), // Restart after N iterations without improvement
        .restart_count              = 0,
        .best_board_state           = NULL,
    };

    // Allocate tabu list
    session.tabu_list = (hpp_tabu_entry*)malloc(sizeof(hpp_tabu_entry) * tabu_capacity);
    if (session.tabu_list == NULL)
    {
        LOG_ERROR("Failed to allocate tabu list");
        return SOLVER_ERROR;
    }

    // Initialize incremental cost table
    session.incremental_cost = incremental_cost_init(board);
    if (session.incremental_cost == NULL)
    {
        LOG_ERROR("Failed to initialize incremental cost");
        free(session.tabu_list);
        return SOLVER_ERROR;
    }

    // Allocate best board state storage
    session.best_board_state = allocate_board_state(board->size);
    if (session.best_board_state == NULL)
    {
        LOG_ERROR("Failed to allocate best board state");
        free(session.tabu_list);
        incremental_cost_destroy(&session.incremental_cost);
        return SOLVER_ERROR;
    }
    save_board_state(session.best_board_state, board);

    LOG_INFO("Tabu search initialized with tenure=%u, restart_threshold=%lu",
             session.tabu_tenure,
             session.restart_threshold);

    if (board->block_size == 0)
    {
        LOG_ERROR("Board block_size is not initialized");
        return SOLVER_ERROR;
    }

    if (config != NULL)
    {
        session.config            = *config;
        session.progress_cb       = config->progress_sink.callback;
        session.progress_userdata = config->progress_sink.userdata;
        session.progress_every_n  = config->progress_sink.progress_every_n > 0
                                        ? config->progress_sink.progress_every_n
                                        : update_rate;
    }

    // Open moves log file if configured
    if (config != NULL && config->moves_log_file != NULL)
    {
        session.moves_log = fopen(config->moves_log_file, "w");
        if (session.moves_log == NULL)
        {
            LOG_WARN("Failed to open moves log file: %s", config->moves_log_file);
        }
        else
        {
            LOG_INFO("Moves log enabled: %s", config->moves_log_file);
            fprintf(session.moves_log, "iteration,move_type,cell1,cell2\n");
            fflush(session.moves_log);
        }
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

        // Check if we should restart (stuck in local minimum)
        if (session.iteration - session.last_improvement_iteration >= session.restart_threshold)
        {
            perform_restart(&session);
        }

        if (emit_progress(&session) != 0)
        {
            LOG_WARN("Solver aborted by callback");
            free(session.tabu_list);
            incremental_cost_destroy(&session.incremental_cost);
            free_board_state(session.best_board_state, board->size);
            if (session.moves_log != NULL)
                fclose(session.moves_log);
            return SOLVER_ABORTED;
        }
    }

    timer_stop(&session.timer);

    // Restore best solution found
    if (session.current_cost.violations > 0 &&
        session.best_cost.violations < session.current_cost.violations)
    {
        restore_board_state(board, session.best_board_state);
        session.current_cost = session.best_cost;
        LOG_INFO("Restored best solution found (violations=%zu)", session.best_cost.violations);
    }

    free(session.tabu_list);
    incremental_cost_destroy(&session.incremental_cost);
    free_board_state(session.best_board_state, board->size);

    if (session.moves_log != NULL)
        fclose(session.moves_log);

    if (session.restart_count > 0)
    {
        LOG_INFO("Total restarts performed: %u", session.restart_count);
    }

    // SANITY CHECK: Verify actual board cost matches tracked cost
    hpp_cost actual_cost = compute_cost(board);
    if (session.current_cost.violations != actual_cost.violations)
    {
        LOG_ERROR("COST MISMATCH! Tracked: %zu, Actual: %zu (diff: %d)",
                  session.current_cost.violations,
                  actual_cost.violations,
                  (int)actual_cost.violations - (int)session.current_cost.violations);
    }

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

/*==============================================================================
 * PRIVATE HELPER IMPLEMENTATIONS
 *==============================================================================*/

/**
 * Allocate a 2D array to store board state.
 */
static uint8_t** allocate_board_state(size_t size)
{
    uint8_t** state = (uint8_t**)malloc(size * sizeof(uint8_t*));
    if (state == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < size; ++i)
    {
        state[i] = (uint8_t*)malloc(size * sizeof(uint8_t));
        if (state[i] == NULL)
        {
            // Clean up already allocated rows
            for (size_t j = 0; j < i; ++j)
            {
                free(state[j]);
            }
            free(state);
            return NULL;
        }
    }

    return state;
}

/**
 * Free board state storage.
 */
static void free_board_state(uint8_t** state, size_t size)
{
    if (state == NULL)
    {
        return;
    }

    for (size_t i = 0; i < size; ++i)
    {
        free(state[i]);
    }
    free(state);
}

/**
 * Save current board configuration to state storage.
 */
static void save_board_state(uint8_t** dest, hpp_board* src)
{
    for (size_t row = 0; row < src->size; ++row)
    {
        for (size_t col = 0; col < src->size; ++col)
        {
            dest[row][col] = src->cells[row][col];
        }
    }
}

/**
 * Restore board configuration from state storage.
 */
static void restore_board_state(hpp_board* dest, uint8_t** src)
{
    for (size_t row = 0; row < dest->size; ++row)
    {
        for (size_t col = 0; col < dest->size; ++col)
        {
            dest->cells[row][col] = src[row][col];
        }
    }
}

/**
 * Perform a random restart: re-randomize the board and clear tabu list.
 */
static void perform_restart(hpp_solver_session* session)
{
    session->restart_count++;

    LOG_INFO("Random restart #%u at iteration %lu (best_cost=%zu, current_cost=%zu)",
             session->restart_count,
             session->iteration,
             session->best_cost.violations,
             session->current_cost.violations);

    // Re-randomize the board
    randomize_board(session->board);

    // Recompute cost after randomization
    session->current_cost = compute_cost(session->board);

    // Clear tabu list to allow exploration of new region
    session->tabu_count = 0;

    // Rebuild incremental cost table
    incremental_cost_destroy(&session->incremental_cost);
    session->incremental_cost = incremental_cost_init(session->board);

    // Reset improvement tracking
    session->last_improvement_iteration = session->iteration;
}

/**
 * Check if a move is in the tabu list.
 * Returns 1 if tabu, 0 if not tabu.
 */
static int is_tabu(const hpp_solver_session* session, const hpp_move* move)
{
    for (size_t i = 0; i < session->tabu_count; ++i)
    {
        const hpp_tabu_entry* entry = &session->tabu_list[i];

        // Check if move matches (considering both ASSIGN and SWAP)
        int moves_match = 0;
        if (move->kind == MOVE_ASSIGN && entry->move.kind == MOVE_ASSIGN)
        {
            moves_match = (move->payload.assign.row == entry->move.payload.assign.row &&
                           move->payload.assign.col == entry->move.payload.assign.col);
        }
        else if (move->kind == MOVE_SWAP && entry->move.kind == MOVE_SWAP)
        {
            // Match both directions of swap
            moves_match = ((move->payload.swap.row1 == entry->move.payload.swap.row1 &&
                            move->payload.swap.col1 == entry->move.payload.swap.col1 &&
                            move->payload.swap.row2 == entry->move.payload.swap.row2 &&
                            move->payload.swap.col2 == entry->move.payload.swap.col2) ||
                           (move->payload.swap.row1 == entry->move.payload.swap.row2 &&
                            move->payload.swap.col1 == entry->move.payload.swap.col2 &&
                            move->payload.swap.row2 == entry->move.payload.swap.row1 &&
                            move->payload.swap.col2 == entry->move.payload.swap.col1));
        }

        if (moves_match && entry->expiration_iteration > session->iteration)
        {
            return 1; // Move is tabu
        }
    }

    return 0; // Move is not tabu
}

/**
 * Add a move to the tabu list.
 * Returns 0 on success, -1 on failure.
 */
static int add_tabu(hpp_solver_session* session, const hpp_move* move)
{
    // Prune expired moves first
    prune_tabu(session);

    // Check if we have capacity
    if (session->tabu_count >= session->tabu_capacity)
    {
        // Remove oldest entry to make room
        for (size_t i = 0; i < session->tabu_count - 1; ++i)
        {
            session->tabu_list[i] = session->tabu_list[i + 1];
        }
        session->tabu_count--;
    }

    // Add move to end of list
    session->tabu_list[session->tabu_count].move = *move;
    session->tabu_list[session->tabu_count].expiration_iteration =
        session->iteration + session->tabu_tenure;
    session->tabu_count++;

    return 0;
}

/**
 * Remove expired entries from the tabu list.
 */
static void prune_tabu(hpp_solver_session* session)
{
    size_t write_idx = 0;
    for (size_t read_idx = 0; read_idx < session->tabu_count; ++read_idx)
    {
        if (session->tabu_list[read_idx].expiration_iteration > session->iteration)
        {
            if (write_idx != read_idx)
            {
                session->tabu_list[write_idx] = session->tabu_list[read_idx];
            }
            write_idx++;
        }
    }
    session->tabu_count = write_idx;
}

/**
 * Ensure move storage capacity can hold at least one more move.
 */
static int ensure_move_capacity(hpp_move** moves, size_t* capacity, size_t move_count)
{
    if (move_count < *capacity)
    {
        return 0;
    }

    *capacity *= 2;
    hpp_move* new_moves = (hpp_move*)realloc(*moves, *capacity * sizeof(hpp_move));
    if (new_moves == NULL)
    {
        return -1;
    }
    *moves = new_moves;
    return 0;
}

static int append_assign_move(hpp_move** moves,
                              size_t*    move_count,
                              size_t*    capacity,
                              uint16_t   row,
                              uint16_t   col,
                              uint8_t    old_value,
                              uint8_t    new_value)
{
    if (ensure_move_capacity(moves, capacity, *move_count) != 0)
    {
        return -1;
    }

    (*moves)[*move_count].kind                     = MOVE_ASSIGN;
    (*moves)[*move_count].payload.assign.row       = row;
    (*moves)[*move_count].payload.assign.col       = col;
    (*moves)[*move_count].payload.assign.old_value = old_value;
    (*moves)[*move_count].payload.assign.new_value = new_value;
    (*move_count)++;

    return 0;
}

static int append_swap_move(hpp_move** moves,
                            size_t*    move_count,
                            size_t*    capacity,
                            uint16_t   row_source,
                            uint16_t   col_source,
                            uint16_t   row_destination,
                            uint16_t   col_destination)
{
    if (ensure_move_capacity(moves, capacity, *move_count) != 0)
    {
        return -1;
    }

    (*moves)[*move_count].kind              = MOVE_SWAP;
    (*moves)[*move_count].payload.swap.row1 = row_source;
    (*moves)[*move_count].payload.swap.col1 = col_source;
    (*moves)[*move_count].payload.swap.row2 = row_destination;
    (*moves)[*move_count].payload.swap.col2 = col_destination;
    (*move_count)++;

    return 0;
}

static int generate_assign_moves(const hpp_board* board,
                                 hpp_move**       moves,
                                 size_t*          move_count,
                                 size_t*          capacity)
{
    for (size_t row = 0; row < board->size; ++row)
    {
        for (size_t col = 0; col < board->size; ++col)
        {
            if (is_fixed(board->fixed, row, col, board->size))
            {
                continue;
            }

            uint8_t old_value = board->cells[row][col];
            for (size_t value = 1; value <= board->size; ++value)
            {
                uint8_t new_value = (uint8_t)value;
                if (new_value == old_value)
                {
                    continue;
                }

                if (append_assign_move(moves,
                                       move_count,
                                       capacity,
                                       (uint16_t)row,
                                       (uint16_t)col,
                                       old_value,
                                       new_value) != 0)
                {
                    return -1;
                }
            }
        }
    }

    return 0;
}

static int generate_row_swap_moves(const hpp_board* board,
                                   hpp_move**       moves,
                                   size_t*          move_count,
                                   size_t*          capacity)
{
    for (size_t row = 0; row < board->size; ++row)
    {
        for (size_t source_col = 0; source_col < board->size; ++source_col)
        {
            if (is_fixed(board->fixed, row, source_col, board->size))
            {
                continue;
            }

            for (size_t destination_col = source_col + 1; destination_col < board->size;
                 ++destination_col)
            {
                if (is_fixed(board->fixed, row, destination_col, board->size))
                {
                    continue;
                }

                if (append_swap_move(moves,
                                     move_count,
                                     capacity,
                                     (uint16_t)row,
                                     (uint16_t)source_col,
                                     (uint16_t)row,
                                     (uint16_t)destination_col) != 0)
                {
                    return -1;
                }
            }
        }
    }

    return 0;
}

static int generate_block_swaps_from_cell(const hpp_board* board,
                                          hpp_move**       moves,
                                          size_t*          move_count,
                                          size_t*          capacity,
                                          size_t           start_row,
                                          size_t           start_col,
                                          size_t           source_sub_row,
                                          size_t           source_sub_col)
{
    size_t source_row = start_row + source_sub_row;
    size_t source_col = start_col + source_sub_col;
    if (is_fixed(board->fixed, source_row, source_col, board->size))
    {
        return 0;
    }

    size_t source_flat = (source_sub_row * board->block_size) + source_sub_col;
    for (size_t destination_flat = source_flat + 1;
         destination_flat < board->block_size * board->block_size;
         ++destination_flat)
    {
        size_t destination_row = start_row + (destination_flat / board->block_size);
        size_t destination_col = start_col + (destination_flat % board->block_size);
        if (is_fixed(board->fixed, destination_row, destination_col, board->size))
        {
            continue;
        }

        if (append_swap_move(moves,
                             move_count,
                             capacity,
                             (uint16_t)source_row,
                             (uint16_t)source_col,
                             (uint16_t)destination_row,
                             (uint16_t)destination_col) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int generate_block_swap_moves(const hpp_board* board,
                                     hpp_move**       moves,
                                     size_t*          move_count,
                                     size_t*          capacity)
{
    size_t num_blocks = board->size / board->block_size;
    for (size_t block_row = 0; block_row < num_blocks; ++block_row)
    {
        for (size_t block_col = 0; block_col < num_blocks; ++block_col)
        {
            size_t start_row = block_row * board->block_size;
            size_t start_col = block_col * board->block_size;

            for (size_t source_sub_row = 0; source_sub_row < board->block_size; ++source_sub_row)
            {
                for (size_t source_sub_col = 0; source_sub_col < board->block_size;
                     ++source_sub_col)
                {
                    if (generate_block_swaps_from_cell(board,
                                                       moves,
                                                       move_count,
                                                       capacity,
                                                       start_row,
                                                       start_col,
                                                       source_sub_row,
                                                       source_sub_col) != 0)
                    {
                        return -1;
                    }
                }
            }
        }
    }

    return 0;
}

/**
 * Generate all valid ASSIGN and SWAP moves for the current board state.
 * Allocates heap memory for the move array; caller must free() it.
 *
 * @param board the board to generate moves for
 * @param out_count output parameter for the number of moves generated
 * @return pointer to heap-allocated array of moves, or NULL if allocation fails
 */
static hpp_move* create_neighbourhood(const hpp_board* board, size_t* out_count)
{
    *out_count = 0;

    if (board->block_size == 0)
    {
        return NULL;
    }

    const size_t margin   = 100;
    size_t       capacity = (board->size * board->size * board->size) +
                      (board->size * board->size * board->size) + margin;

    hpp_move* moves = (hpp_move*)malloc(capacity * sizeof(hpp_move));
    if (moves == NULL)
    {
        return NULL;
    }

    size_t move_count = 0;

    if (/*generate_assign_moves(board, &moves, &move_count, &capacity) != 0 || */
        generate_row_swap_moves(board, &moves, &move_count, &capacity) != 0 /*||
        generate_block_swap_moves(board, &moves, &move_count, &capacity) != 0*/)
    {
        free(moves);
        return NULL;
    }

    *out_count = move_count;
    return moves;
}

static void apply_move(hpp_board* board, const hpp_move* move)
{
    if (move->kind == MOVE_ASSIGN)
    {
        board->cells[move->payload.assign.row][move->payload.assign.col] =
            move->payload.assign.new_value;
        return;
    }

    uint16_t source_row      = move->payload.swap.row1;
    uint16_t source_col      = move->payload.swap.col1;
    uint16_t destination_row = move->payload.swap.row2;
    uint16_t destination_col = move->payload.swap.col2;

    uint8_t temp                                   = board->cells[source_row][source_col];
    board->cells[source_row][source_col]           = board->cells[destination_row][destination_col];
    board->cells[destination_row][destination_col] = temp;
}

static void undo_move(hpp_board* board, const hpp_move* move)
{
    if (move->kind == MOVE_ASSIGN)
    {
        board->cells[move->payload.assign.row][move->payload.assign.col] =
            move->payload.assign.old_value;
        return;
    }

    uint16_t source_row      = move->payload.swap.row1;
    uint16_t source_col      = move->payload.swap.col1;
    uint16_t destination_row = move->payload.swap.row2;
    uint16_t destination_col = move->payload.swap.col2;

    uint8_t temp                                   = board->cells[source_row][source_col];
    board->cells[source_row][source_col]           = board->cells[destination_row][destination_col];
    board->cells[destination_row][destination_col] = temp;
}

static int probe_best_improving(hpp_solver_session* session,
                                const hpp_move*     moves,
                                size_t              move_count,
                                hpp_move*           best_move,
                                hpp_cost*           best_cost)
{
    int      found_improving_move = 0;
    int      found_any_move       = 0;
    hpp_cost best_cost_found      = {.violations = UINT_MAX, .depth_score = 1e9};
    hpp_move best_move_found      = {0};

// Parallelize the search for the best move across all threads
#pragma omp parallel
    {
        // Thread-local best move and cost
        hpp_cost thread_best_cost = {.violations = UINT_MAX, .depth_score = 1e9};
        hpp_move thread_best_move = {0};
        int      thread_found_any = 0;

#pragma omp for schedule(dynamic, 256) nowait
        for (size_t index = 0; index < move_count; ++index)
        {
            const hpp_move* candidate_move = &moves[index];

            // Check tabu list (with aspiration criteria)
            // Note: is_tabu() is read-only, so thread-safe
            int is_move_tabu = is_tabu(session, candidate_move);
            if (is_move_tabu && thread_found_any &&
                thread_best_cost.violations >= session->best_cost.violations)
            {
                // Skip tabu moves unless they beat the global best cost
                continue;
            }

            // Compute cost delta without modifying board
            // Note: incremental_cost_delta functions are read-only, so thread-safe
            int32_t delta = 0;
            if (candidate_move->kind == MOVE_ASSIGN)
            {
                delta = incremental_cost_delta_assign(session->incremental_cost,
                                                      session->board,
                                                      candidate_move->payload.assign.row,
                                                      candidate_move->payload.assign.col,
                                                      candidate_move->payload.assign.new_value);
            }
            else if (candidate_move->kind == MOVE_SWAP)
            {
                delta = incremental_cost_delta_swap(session->incremental_cost,
                                                    session->board,
                                                    candidate_move->payload.swap.row1,
                                                    candidate_move->payload.swap.col1,
                                                    candidate_move->payload.swap.row2,
                                                    candidate_move->payload.swap.col2);
            }

            // Compute candidate cost
            hpp_cost candidate_cost;
            candidate_cost.violations =
                (uint32_t)((int32_t)session->current_cost.violations + delta);
            // Approximate depth_score (simplified: scale by violation change)
            candidate_cost.depth_score =
                session->current_cost.depth_score +
                (double)delta * (session->current_cost.depth_score /
                                 (double)(session->current_cost.violations + 1));

            // First move or better than thread's best found?
            if (!thread_found_any || cost_compare(&candidate_cost, &thread_best_cost) < 0)
            {
                thread_best_cost = candidate_cost;
                thread_best_move = *candidate_move;
                thread_found_any = 1;
            }
        }

// Combine thread-local results into global best
#pragma omp critical
        {
            if (thread_found_any)
            {
                if (!found_any_move || cost_compare(&thread_best_cost, &best_cost_found) < 0)
                {
                    best_cost_found = thread_best_cost;
                    best_move_found = thread_best_move;
                    found_any_move  = 1;
                }
            }
        }
    }

    // Check if best move is improving vs current cost
    if (found_any_move)
    {
        *best_cost = best_cost_found;
        *best_move = best_move_found;

        if (cost_compare(&best_cost_found, &session->current_cost) < 0)
        {
            found_improving_move = 1;
        }
    }

    return found_improving_move;
}

static int probe_first_improving(hpp_solver_session* session,
                                 const hpp_move*     moves,
                                 size_t              move_count,
                                 hpp_move*           best_move,
                                 hpp_cost*           best_cost)
{
    int      found_improving_move = 0;
    int      found_any_move       = 0;
    hpp_cost best_cost_found      = {.violations = UINT_MAX, .depth_score = 1e9};

    for (size_t index = 0; index < move_count; ++index)
    {
        const hpp_move* candidate_move = &moves[index];

        // Check tabu list (with aspiration criteria)
        int is_move_tabu = is_tabu(session, candidate_move);
        if (is_move_tabu && found_any_move &&
            best_cost_found.violations >= session->best_cost.violations)
        {
            // Skip tabu moves unless they beat the global best
            continue;
        }

        // Compute cost delta without modifying board
        int32_t delta = 0;
        if (candidate_move->kind == MOVE_ASSIGN)
        {
            delta = incremental_cost_delta_assign(session->incremental_cost,
                                                  session->board,
                                                  candidate_move->payload.assign.row,
                                                  candidate_move->payload.assign.col,
                                                  candidate_move->payload.assign.new_value);
        }
        else if (candidate_move->kind == MOVE_SWAP)
        {
            delta = incremental_cost_delta_swap(session->incremental_cost,
                                                session->board,
                                                candidate_move->payload.swap.row1,
                                                candidate_move->payload.swap.col1,
                                                candidate_move->payload.swap.row2,
                                                candidate_move->payload.swap.col2);
        }

        // Compute candidate cost
        hpp_cost candidate_cost;
        candidate_cost.violations = (uint32_t)((int32_t)session->current_cost.violations + delta);
        // Approximate depth_score
        candidate_cost.depth_score =
            session->current_cost.depth_score +
            (double)delta * (session->current_cost.depth_score /
                             (double)(session->current_cost.violations + 1));

        // First move or better than best?
        if (!found_any_move || cost_compare(&candidate_cost, &best_cost_found) < 0)
        {
            best_cost_found = candidate_cost;
            *best_cost      = candidate_cost;
            *best_move      = *candidate_move;
            found_any_move  = 1;

            // For first improving, return on first improvement
            if (cost_compare(&candidate_cost, &session->current_cost) < 0)
            {
                return 1;
            }
        }
    }

    // Return whether best move found is improving vs current
    if (found_any_move && cost_compare(&best_cost_found, &session->current_cost) < 0)
    {
        found_improving_move = 1;
    }

    return found_improving_move;
}

static int probe_neighbourhood(hpp_solver_session* session,
                               const hpp_move*     moves,
                               size_t              move_count,
                               hpp_probe_strategy  strategy,
                               hpp_move*           best_move,
                               hpp_cost*           best_cost)
{
    if (strategy == PROBE_STRATEGY_FIRST_IMPROVING)
    {
        return probe_first_improving(session, moves, move_count, best_move, best_cost);
    }

    return probe_best_improving(session, moves, move_count, best_move, best_cost);
}

/**
 * Compute current board cost (violations/conflicts).
 * Counts all duplicate values within columns and blocks.
 * Rows are always valid due to row-swap moves and proper initialization.
 */
static hpp_cost compute_cost(const hpp_board* board)
{
    size_t violations  = 0;
    double depth_score = 0.0;

    // Count conflicts in all columns and weight by position (left prioritized)
    for (size_t col = 0; col < board->size; ++col)
    {
        uint32_t col_conflicts = count_conflicts_in_col(board, col);
        violations += col_conflicts;
        // Weight: columns on left have higher weight
        double weight = (double)(board->size - col);
        depth_score += col_conflicts * weight;
    }

    // Count conflicts in all blocks and weight by position (top-left prioritized)
    size_t num_blocks = board->size / board->block_size;
    size_t block_idx  = 0;
    for (size_t block_row = 0; block_row < num_blocks; ++block_row)
    {
        for (size_t block_col = 0; block_col < num_blocks; ++block_col)
        {
            uint32_t block_conflicts = count_conflicts_in_block(board, block_row, block_col);
            violations += block_conflicts;
            // Weight: blocks at top-left have higher weight
            double weight = (double)(num_blocks * num_blocks - block_idx);
            depth_score += block_conflicts * weight;
            block_idx++;
        }
    }

    hpp_cost cost = {.violations = violations, .depth_score = depth_score};
    return cost;
}

/**
 * Lexicographical comparison of two costs.
 * Returns: -1 if cost1 is better, 0 if equal, 1 if cost2 is better
 */
static inline int cost_compare(const hpp_cost* cost1, const hpp_cost* cost2)
{
    // Primary: compare violations
    if (cost1->violations != cost2->violations)
    {
        return cost1->violations < cost2->violations ? -1 : 1;
    }

    // Secondary: compare depth_score (lower is better)
    if (cost1->depth_score < cost2->depth_score)
    {
        return -1;
    }
    else if (cost1->depth_score > cost2->depth_score)
    {
        return 1;
    }

    return 0; // Equal
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
    if (session->progress_every_n == 0)
    {
        return 0;
    }
    if (session->iteration % session->progress_every_n != 0)
    {
        return 0;
    }

    // Calculate elapsed time from start to now
    struct timespec now;
#ifdef CLOCK_MONOTONIC_RAW
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
#else
    clock_gettime(CLOCK_MONOTONIC, &now);
#endif

    double elapsed_seconds = ((double)(now.tv_sec - session->timer.start.tv_sec) * TIMER_NS_IN_S +
                              (double)(now.tv_nsec - session->timer.start.tv_nsec)) /
                             TIMER_NS_IN_S;

    hpp_solver_progress progress = {
        .sequence        = session->iteration,
        .iteration       = session->iteration,
        .cost            = session->current_cost,
        .best_cost       = session->best_cost,
        .latest_move     = session->latest_move,
        .elapsed_seconds = elapsed_seconds,
    };

    int ret = session->progress_cb(&progress, session->progress_userdata);
    return ret;
}

/**
 * Log a move to the moves log file for cycle detection.
 * Format: iteration,move_type,cell1,cell2
 */
static void log_move(hpp_solver_session* session, const hpp_move* move)
{
    if (session->moves_log == NULL)
    {
        return;
    }

    if (move->kind == MOVE_ASSIGN)
    {
        fprintf(session->moves_log,
                "%lu,ASSIGN,(%u,%u),==%u\n",
                session->iteration,
                move->payload.assign.row,
                move->payload.assign.col,
                move->payload.assign.new_value);
    }
    else if (move->kind == MOVE_SWAP)
    {
        fprintf(session->moves_log,
                "%lu,SWAP,(%u,%u),(%u,%u)\n",
                session->iteration,
                move->payload.swap.row1,
                move->payload.swap.col1,
                move->payload.swap.row2,
                move->payload.swap.col2);
    }

    fflush(session->moves_log);
}

/**
 * Take one solver iteration (step/probe/neighborhood search).
 * Generates neighbourhood, evaluates all moves, and applies best improving move if found.
 */
static void solver_step(hpp_solver_session* session)
{
    const hpp_probe_strategy strategy = PROBE_STRATEGY_BEST_IMPROVING;

    size_t    move_count = 0;
    hpp_move* moves      = create_neighbourhood(session->board, &move_count);

    if (moves == NULL)
    {
        session->iteration++;
        return;
    }

    if (move_count == 0)
    {
        free(moves);
        session->iteration++;
        return;
    }

    hpp_move best_move              = {0};
    hpp_cost best_cost_in_iteration = session->current_cost;
    probe_neighbourhood(session, moves, move_count, strategy, &best_move, &best_cost_in_iteration);

    // Always apply the best move found (improving or not) to escape local minima
    // The tabu list prevents cycling back to recently visited states
    if (best_move.kind != 0)
    {
        apply_move(session->board, &best_move);
        log_move(session, &best_move);

        // Update incremental cost tracking
        incremental_cost_apply_move(session->incremental_cost, session->board, &best_move);

        // IMPORTANT: Use the TRUE cost from the incremental system, not the delta estimate
        // The delta estimate can accumulate errors; we trust the fresh recomputation
        session->current_cost.violations = session->incremental_cost->total_violations;
        // depth_score is approximated but good for ordering
        session->current_cost.depth_score = best_cost_in_iteration.depth_score;

        session->latest_move = best_move;

        // Add move to tabu list
        add_tabu(session, &best_move);

        // Track global best cost and save best board state
        if (best_cost_in_iteration.violations < session->best_cost.violations)
        {
            session->best_cost = best_cost_in_iteration;
            save_board_state(session->best_board_state, session->board);
            session->last_improvement_iteration = session->iteration;

            LOG_DEBUG("New best cost found: %zu violations at iteration %lu",
                      session->best_cost.violations,
                      session->iteration);
        }
    }

    // Free neighbourhood moves
    free(moves);

    session->iteration++;
}
