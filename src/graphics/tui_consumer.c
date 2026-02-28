#include "graphics/tui_consumer.h"

#include "solver/progress.h"
#include "solver/sudoku.h"
#include "utils/logger.h"
#include "utils/timing.h"

#include <pthread.h>
#include <stdbool.h>
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
        // TODO: render consumer->board to terminal
        // TODO: display consumer->latest_progress stats
        nanosleep(&sleep_time, NULL);
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
    consumer->should_stop        = false;
    const int fps                = 20; // 20 Hz render rate
    const int fps_timer          = TIMER_MS_IN_S / fps;
    consumer->render_interval_ms = fps_timer;

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

    consumer->should_stop = true;
    pthread_join(consumer->render_thread, NULL);

    destroy_board(&consumer->board);
    pthread_mutex_destroy(&consumer->progress_mutex);

    free(consumer);
    *consumer_ptr = NULL;

    LOG_INFO("TUI consumer destroyed");
}

hpp_progress_sink_config tui_consumer_sink(hpp_tui_consumer* consumer)
{
    const int                update_rate = 100;
    hpp_progress_sink_config sink        = {
               .callback         = tui_consumer_callback,
               .userdata         = consumer,
               .progress_every_n = update_rate, // Emit progress every 100 solver iterations
    };
    return sink;
}

void tui_consumer_finalize(hpp_tui_consumer* consumer, const hpp_solver_progress* final_progress)
{
    if (consumer == NULL)
    {
        return;
    }

    if (final_progress != NULL)
    {
        pthread_mutex_lock(&consumer->progress_mutex);
        memcpy(&consumer->latest_progress, final_progress, sizeof(*final_progress));
        pthread_mutex_unlock(&consumer->progress_mutex);
    }

    // TODO: display final board/result, maybe wait for user input (ESC key)
    LOG_INFO("Solver finished; final board:");
    print_board(consumer->board);
}
