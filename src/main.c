#include "graphics/tui_consumer.h"
#include "parser/parser.h"
#include "solver/solver.h"
#include "solver/sudoku.h"
#include "utils/logger.h"
#include "utils/timing.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * Parse command-line arguments.
 *
 * @param argc argument count.
 * @param argv argument values.
 * @param enable_ui output flag for --ui mode.
 */
static void parse_args(int argc, char* argv[], bool* enable_ui)
{
    *enable_ui = false;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--ui") == 0)
        {
            *enable_ui = true;
        }
    }
}

int main(int argc, char* argv[])
{
    bool enable_ui = false;
    parse_args(argc, argv, &enable_ui);

    // logger_set_level(LOG_LEVEL_DEBUG);
    // logger_set_verbosity(0);

    hpp_timer timer;
    timer_start(&timer);

    LOG_INFO("Loading board from board.dat");
    hpp_board* initial_board = parse_file("board.dat");
    if (initial_board == NULL)
    {
        LOG_ERROR("Failed to parse board");
        return 1;
    }

    LOG_INFO("Received board:");
    print_board(initial_board);

    // Set up solver configuration
    const int         update_rate   = 100;
    hpp_solver_config solver_config = {
        .max_iterations = 0, // No limit (0 = unlimited)
        .progress_sink  = {.callback = NULL, .userdata = NULL, .progress_every_n = update_rate},
    };

    // Set up TUI consumer if requested
    hpp_tui_consumer* tui = NULL;
    if (enable_ui)
    {
        LOG_INFO("Starting TUI");
        tui = tui_consumer_create(initial_board->size);
        if (tui != NULL)
        {
            solver_config.progress_sink = tui_consumer_sink(tui);
        }
        else
        {
            LOG_WARN("Failed to create TUI consumer; running headless");
        }
    }

    // Run solver
    LOG_INFO("Starting solver");
    hpp_solver_status status = solve(initial_board, &solver_config);

    // Finalize TUI if running
    if (tui != NULL)
    {
        tui_consumer_finalize(tui, NULL);
        tui_consumer_destroy(&tui);
    }
    else
    {
        // Headless mode: print final board
        LOG_INFO("Final board:");
        print_board(initial_board);
    }

    // Clean up
    destroy_board(&initial_board);

    timer_stop(&timer);

    const char* status_str;
    switch (status)
    {
        case SOLVER_SUCCESS:
            status_str = "SUCCESS";
            break;
        case SOLVER_UNSOLVED:
            status_str = "UNSOLVED";
            break;
        case SOLVER_ABORTED:
            status_str = "ABORTED";
            break;
        case SOLVER_ERROR:
            status_str = "ERROR";
            break;
    }

    LOG_INFO("Solver finished with status: %s", status_str);
    LOG_INFO("Total time: %.4lf seconds", timer_s(&timer));

    return (status == SOLVER_SUCCESS) ? 0 : 1;
}