#include "graphics/tui_consumer.h"

#include "solver/progress.h"
#include "solver/sudoku.h"
#include "utils/logger.h"
#include "utils/timing.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Internal TUI consumer state.
 */
typedef struct TUIConsumer
{
    /** Mirrored board state (owned by consumer). */
    hpp_board* board;

    /** Latest progress snapshot (updated by callback). */
    hpp_solver_progress latest_progress;

    /** Synchronization: protects latest_progress. */
    pthread_mutex_t progress_mutex;

    /** Render thread handle. */
    pthread_t render_thread;

    /** Signal for render thread to stop. */
    bool should_stop;

    /** Track if render thread has been joined. */
    bool render_thread_joined;

    /** Render frequency (ms). */
    uint32_t render_interval_ms;
} hpp_tui_consumer;

/**
 * Progress callback: called by solver to report progress.
 * Updates consumer's mirrored board and latest progress.
 * Must be very fast and non-blocking.
 */
static int tui_consumer_callback(const hpp_solver_progress* progress, void* userdata)
{
    hpp_tui_consumer* consumer = (hpp_tui_consumer*)userdata;

    if (consumer == NULL || progress == NULL)
    {
        return -1;
    }

    // Apply latest move to mirrored board (if present)
    // TODO: apply move.payload to consumer->board if move is valid

    // Update latest progress snapshot
    pthread_mutex_lock(&consumer->progress_mutex);
    memcpy(&consumer->latest_progress, progress, sizeof(*progress));
    pthread_mutex_unlock(&consumer->progress_mutex);

    return 0; // Continue solving
}

static void print_move_info(const hpp_move* move)
{
    // Check if move is empty (no move applied in this iteration)
    if (move->kind == MOVE_ASSIGN && move->payload.assign.row == 0 &&
        move->payload.assign.col == 0 && move->payload.assign.old_value == 0 &&
        move->payload.assign.new_value == 0)
    {
        printf("  Move: (no improving move found)\033[K\n");
    }
    else if (move->kind == MOVE_ASSIGN)
    {
        printf("  Move: ASSIGN [row=%u, col=%u] old_value=%u → new_value=%u\033[K\n",
               move->payload.assign.row,
               move->payload.assign.col,
               move->payload.assign.old_value,
               move->payload.assign.new_value);
    }
    else if (move->kind == MOVE_SWAP)
    {
        printf("  Move: SWAP [(%u,%u) ↔ (%u,%u)]\033[K\n",
               move->payload.swap.row1,
               move->payload.swap.col1,
               move->payload.swap.row2,
               move->payload.swap.col2);
    }
}

static void print_progress_placeholder(void)
{
    // Print 7 blank lines to reserve space for progress display
    printf("\n");
    printf("\n");
    printf("\n");
    printf("\n");
    printf("\n");
    printf("\n");
    printf("\n");
    fflush(stdout);
}

static void print_progress_info(const hpp_tui_consumer* consumer)
{
    // Move cursor up 7 lines to overwrite previous output
    printf("\033[7A");

    printf("=== SOLVER PROGRESS ===\033[K\n");
    printf("  Iteration: %lu\033[K\n", consumer->latest_progress.iteration);
    printf("  Elapsed:   %.2lf s\033[K\n", consumer->latest_progress.elapsed_seconds);
    printf("  Current Cost (violations): %u\033[K\n", consumer->latest_progress.cost.violations);
    printf("  Best Cost (violations):    %u\033[K\n",
           consumer->latest_progress.best_cost.violations);
    print_move_info(&consumer->latest_progress.latest_move);
    printf("======================\033[K\n");
    fflush(stdout);
}

/**
 * Render thread: periodically updates TUI display.
 * Reads from consumer's mirrored board and latest progress.
 */
static void* render_thread_fn(void* arg)
{
    hpp_tui_consumer* consumer = (hpp_tui_consumer*)arg;
    LOG_DEBUG("TUI render thread started");

    const long timeout = TIMER_NS_IN_S / TIMER_MS_IN_S;

    struct timespec sleep_time = {
        .tv_sec  = 0,
        .tv_nsec = consumer->render_interval_ms * timeout,
    };

    while (!consumer->should_stop)
    {
        nanosleep(&sleep_time, NULL);

        // Display progress info
        pthread_mutex_lock(&consumer->progress_mutex);
        print_progress_info(consumer);
        pthread_mutex_unlock(&consumer->progress_mutex);
    }

    LOG_DEBUG("TUI render thread exiting");
    return NULL;
}

hpp_tui_consumer* tui_consumer_create(size_t size)
{
    if (size == 0)
    {
        return NULL;
    }

    hpp_tui_consumer* consumer = (hpp_tui_consumer*)malloc(sizeof(*consumer));
    if (consumer == NULL)
    {
        return NULL;
    }

    consumer->board = create_board(size);
    if (consumer->board == NULL)
    {
        free(consumer);
        return NULL;
    }

    memset(&consumer->latest_progress, 0, sizeof(consumer->latest_progress));
    pthread_mutex_init(&consumer->progress_mutex, NULL);
    consumer->should_stop          = false;
    consumer->render_thread_joined = false;
    const int fps                  = 20; // 20 Hz render rate
    const int fps_timer            = TIMER_MS_IN_S / fps;
    consumer->render_interval_ms   = fps_timer;

    // Print initial placeholder to reserve display space
    print_progress_placeholder();

    // Start render thread
    if (pthread_create(&consumer->render_thread, NULL, render_thread_fn, consumer) != 0)
    {
        destroy_board(&consumer->board);
        pthread_mutex_destroy(&consumer->progress_mutex);
        free(consumer);
        return NULL;
    }

    LOG_INFO("TUI consumer created (board %zux%zu)", size, size);
    return consumer;
}

void tui_consumer_destroy(hpp_tui_consumer** consumer_ptr)
{
    if (consumer_ptr == NULL || *consumer_ptr == NULL)
    {
        return;
    }

    hpp_tui_consumer* consumer = *consumer_ptr;

    // Only join render thread if not already joined in finalize
    if (!consumer->render_thread_joined)
    {
        consumer->should_stop = true;
        pthread_join(consumer->render_thread, NULL);
    }

    destroy_board(&consumer->board);
    pthread_mutex_destroy(&consumer->progress_mutex);

    free(consumer);
    *consumer_ptr = NULL;

    LOG_INFO("TUI consumer destroyed");
}

hpp_progress_sink_config tui_consumer_sink(hpp_tui_consumer* consumer)
{
    const int                update_rate = 1;
    hpp_progress_sink_config sink        = {
               .callback         = tui_consumer_callback,
               .userdata         = consumer,
               .progress_every_n = update_rate, // Emit progress every 100 solver iterations
    };
    return sink;
}

void tui_consumer_finalize(hpp_tui_consumer*          consumer,
                           const hpp_solver_progress* final_progress,
                           const hpp_board*           final_board)
{
    if (consumer == NULL)
    {
        return;
    }

    // Stop render thread first to prevent it from overwriting output
    consumer->should_stop = true;
    pthread_join(consumer->render_thread, NULL);
    consumer->render_thread_joined = true;

    if (final_progress != NULL)
    {
        pthread_mutex_lock(&consumer->progress_mutex);
        memcpy(&consumer->latest_progress, final_progress, sizeof(*final_progress));
        pthread_mutex_unlock(&consumer->progress_mutex);
    }

    // Display final result (now safe from race conditions)
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║     SOLVER FINISHED - FINAL RESULT     ║\n");
    printf("╚════════════════════════════════════════╝\n\n");

    pthread_mutex_lock(&consumer->progress_mutex);
    printf("Final Statistics:\n");
    printf("  Total Iterations: %lu\n", consumer->latest_progress.iteration);
    printf("  Total Time:       %.4lf s\n", consumer->latest_progress.elapsed_seconds);
    printf("  Final Cost:       %u violations\n", consumer->latest_progress.cost.violations);
    printf("  Best Cost:        %u violations\n", consumer->latest_progress.best_cost.violations);
    printf("\nFinal Board:\n");
    pthread_mutex_unlock(&consumer->progress_mutex);

    if (final_board != NULL)
    {
        print_board(final_board);
    }
    else
    {
        printf("(Board not available)\n");
    }
}
