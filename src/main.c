/**
 * @file main.c
 * @brief CLI entrypoint for loading, solving, and emitting Sudoku boards.
 */

#include "data/board.h"
#include "parser/parser.h"
#include "solver/solver.h"
#include "utils/logger.h"
#include "utils/timing.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* =========================================================================
 * File Constants
 * ========================================================================= */

static const char* const default_input_file  = "board.dat";
static const char* const default_output_file = "answer.dat";

/* =========================================================================
 * Forward Declarations
 * ========================================================================= */

static uint32_t    parse_thread_count_arg(const char* value);
static void        parse_args(int          argc,
                              char*        argv[],
                              bool*        enable_ui,
                              const char** moves_log_file,
                              const char** input_file,
                              bool*        enable_benchmark,
                              uint32_t*    thread_count);
static void        write_solution_output(hpp_solver_status status, hpp_board* board, bool is_piped);
static void        configure_logging_for_pipe(bool is_piped);
static const char* get_status_string(hpp_solver_status status);

/* =========================================================================
 * Public API
 * ========================================================================= */

int main(int argc, char* argv[])
{
    /* Parse command-line arguments. */
    bool        enable_ui        = false;
    const char* moves_log_file   = NULL;
    const char* input_file       = NULL;
    bool        enable_benchmark = false;
    uint32_t    thread_count     = 0U;
    parse_args(
        argc, argv, &enable_ui, &moves_log_file, &input_file, &enable_benchmark, &thread_count);

    /* Detect piped output and disable noisy logs in data pipelines. */
    bool is_piped = !isatty(STDOUT_FILENO);
    configure_logging_for_pipe(is_piped);
    if (is_piped)
    {
        enable_ui = false;
    }

    /* Benchmark mode emits machine-readable JSON only. */
    if (enable_benchmark)
    {
        logger_set_level(LOG_LEVEL_NONE);
        enable_ui = false;
    }

    hpp_timer timer;
    timer_start(&timer);

    LOG_INFO("Loading board from %s", input_file);
    hpp_board* initial_board = parse_file(input_file);
    if (initial_board == NULL)
    {
        LOG_ERROR("Failed to parse board");
        return 1;
    }

    /* Display board if not in piped or benchmark mode */
    if (!is_piped && !enable_benchmark)
    {
        printf("Solving the following board:\n");
        print_board(initial_board);
        printf("\n");
    }

    /* Run solver. */
    LOG_INFO("Starting solver");
    hpp_solver_config solver_config = {
        .thread_count   = thread_count,
        .moves_log_file = moves_log_file,
    };
    hpp_solver_status status = solve(initial_board, &solver_config);

    /* Write solution if successful. */
    write_solution_output(status, initial_board, is_piped);

    timer_stop(&timer);

    /* Report results. */
    if (enable_benchmark)
    {
        /* Output benchmark data in JSON format */
        uint64_t elapsed_ns = timer_ns(&timer);
        printf("{\"status\": \"%s\", \"elapsed_ns\": %llu, \"elapsed_s\": %.9lf}\n",
               get_status_string(status),
               (unsigned long long)elapsed_ns,
               timer_s(&timer));
    }
    else
    {
        printf("Solved board:\n");
        print_board(initial_board);
        printf("\n");
        LOG_INFO("Solver finished with status: %s", get_status_string(status));
        LOG_INFO("Total time: %.4lf seconds", timer_s(&timer));
    }

    /* Clean up. */
    destroy_board(&initial_board);

    return (status == SOLVER_SUCCESS) ? 0 : 1;
}

/* =========================================================================
 * Internal Helpers
 * ========================================================================= */

/**
 * @brief Parse an optional thread-count CLI argument.
 *
 * @note Returns `0` for invalid values so caller can keep defaults.
 * @pre `value` may be `NULL`.
 * @post No global state is modified.
 *
 * @param value Raw argument text.
 * @return Parsed thread count, or `0` when invalid/unspecified.
 */
static uint32_t parse_thread_count_arg(const char* value)
{
    if (value == NULL || *value == '\0')
    {
        return 0U;
    }

    char* end_ptr              = NULL;
    errno                      = 0;
    const unsigned long parsed = strtoul(value, &end_ptr, 10);
    if (errno != 0 || end_ptr == value || *end_ptr != '\0' || parsed == 0U || parsed > UINT32_MAX)
    {
        return 0U;
    }

    return (uint32_t)parsed;
}

/**
 * @brief Parse command-line arguments.
 *
 * @note Unknown flags are ignored unless interpreted as input-file argument.
 * @pre Output pointers are non-NULL.
 * @post Output parameters contain parsed CLI state or defaults.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @param enable_ui Output flag for `--ui` mode.
 * @param moves_log_file Output pointer for `--moves-log FILE` mode.
 * @param input_file Output pointer for positional input file path.
 * @param enable_benchmark Output flag for `--benchmark` mode.
 * @param thread_count Output thread count override.
 */
static void parse_args(int          argc,
                       char*        argv[],
                       bool*        enable_ui,
                       const char** moves_log_file,
                       const char** input_file,
                       bool*        enable_benchmark,
                       uint32_t*    thread_count)
{
    *enable_ui        = false;
    *moves_log_file   = NULL;
    *input_file       = default_input_file;
    *enable_benchmark = false;
    *thread_count     = 0U;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--ui") == 0)
        {
            *enable_ui = true;
        }
        else if (strcmp(argv[i], "--benchmark") == 0)
        {
            *enable_benchmark = true;
        }
        else if (strcmp(argv[i], "--moves-log") == 0 && i + 1 < argc)
        {
            *moves_log_file = argv[++i];
        }
        else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc)
        {
            const uint32_t parsed_thread_count = parse_thread_count_arg(argv[++i]);
            if (parsed_thread_count != 0U)
            {
                *thread_count = parsed_thread_count;
            }
        }
        else if (strncmp(argv[i], "--threads=", 10) == 0)
        {
            const uint32_t parsed_thread_count = parse_thread_count_arg(argv[i] + 10);
            if (parsed_thread_count != 0U)
            {
                *thread_count = parsed_thread_count;
            }
        }
        else if (strncmp(argv[i], "--threads ", 10) == 0)
        {
            // Supports accidental combined argv tokens such as "--threads 12".
            const uint32_t parsed_thread_count = parse_thread_count_arg(argv[i] + 10);
            if (parsed_thread_count != 0U)
            {
                *thread_count = parsed_thread_count;
            }
        }
        else if (strcmp(argv[i], "--max-iterations") == 0 && i + 1 < argc)
        {
            // Deprecated and ignored: retained to avoid breaking old CLI scripts.
            i++;
        }
        else if (argv[i][0] != '-')
        {
            /* First non-flag argument is the input file */
            *input_file = argv[i];
        }
    }
}

/**
 * @brief Write solution to output (stdout if piped, answer.dat otherwise).
 *
 * @pre `board != NULL`.
 * @post On success path, serialized solution is written to selected output.
 *
 * @param status Solver status.
 * @param board Solved board.
 * @param is_piped Whether stdout is piped.
 */
static void write_solution_output(hpp_solver_status status, hpp_board* board, bool is_piped)
{
    if (status != SOLVER_SUCCESS)
    {
        return;
    }

    if (is_piped)
    {
        write_board_to_stream(stdout, board, 1);
    }
    else
    {
        LOG_DEBUG("Writing solution to %s", default_output_file);
        if (write_solution(default_output_file, board) != 0)
        {
            LOG_WARN("Failed to write solution file");
        }
    }
}

/**
 * @brief Configure logging based on whether output is piped.
 *
 * Disables logging if output is piped (for clean output in scripts).
 *
 * @pre None.
 * @post Runtime log level may be changed.
 *
 * @param is_piped Whether stdout is piped.
 */
static void configure_logging_for_pipe(bool is_piped)
{
    if (is_piped)
    {
        logger_set_level(LOG_LEVEL_NONE);
    }
}

/**
 * @brief Convert solver status to string representation.
 *
 * @pre None.
 * @post No state is modified.
 *
 * @param status Solver status code.
 * @return Constant status label string.
 *
 * @par Example
 * @code{.c}
 * printf("status=%s\n", get_status_string(SOLVER_SUCCESS));
 * @endcode
 */
static const char* get_status_string(hpp_solver_status status)
{
    switch (status)
    {
        case SOLVER_SUCCESS:
            return "SUCCESS";
        case SOLVER_UNSOLVED:
            return "UNSOLVED";
        case SOLVER_ABORTED:
            return "ABORTED";
        case SOLVER_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}
