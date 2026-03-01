#ifndef GRAPHICS_TUI_CONSUMER_H
#define GRAPHICS_TUI_CONSUMER_H

#include "solver/progress.h"
#include "solver/sink.h"

#include <stdint.h>

// Forward declaration
typedef struct Board hpp_board;

/**
 * Opaque handle to a TUI consumer instance.
 *
 * Manages:
 * - A local mirrored copy of the board state.
 * - A dedicated rendering thread that updates display every few ms.
 * - Synchronized access to latest progress without blocking the solver.
 */
typedef struct TUIConsumer hpp_tui_consumer;

/**
 * Create and start a TUI consumer with a given initial board.
 *
 * Spawns an internal render thread that will periodically refresh the display.
 * The returned consumer can be passed to the solver as a progress sink.
 *
 * @param size board size (e.g., 9 for standard Sudoku).
 * @return newly allocated consumer, or NULL on failure.
 */
hpp_tui_consumer* tui_consumer_create(size_t size);

/**
 * Destroy a TUI consumer and stop its render thread.
 *
 * All resources are freed; the pointer is set to NULL.
 *
 * @param consumer pointer to consumer handle.
 */
void tui_consumer_destroy(hpp_tui_consumer** consumer);

/**
 * Retrieve the progress sink configuration for this consumer.
 *
 * Call this after creating the consumer and pass the result to the solver.
 * The returned config has the consumer set as userdata.
 *
 * @param consumer the TUI consumer handle.
 * @return sink configuration (callback + userdata + cadence).
 */
hpp_progress_sink_config tui_consumer_sink(hpp_tui_consumer* consumer);

/**
 * Signal to the consumer that solving is complete.
 *
 * The render thread will display final state and may wait for user input (esc key).
 * Must be called after the solver finishes.
 *
 * @param consumer the TUI consumer handle.
 * @param final_progress final progress state (may be NULL).
 * @param final_board the solved board to display (or NULL to skip board display).
 */
void tui_consumer_finalize(hpp_tui_consumer*          consumer,
                           const hpp_solver_progress* final_progress,
                           const hpp_board*           final_board);

#endif
