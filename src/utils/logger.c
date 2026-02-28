#include "utils/logger.h"

#include "utils/colors.h"

#include <stdarg.h>
#include <stdio.h>

static hpp_log_level global_log_level = LOG_LEVEL;
static int           global_verbosity = LOG_VERBOSITY;

static const char* level_strings[] = {"DEBG", "INFO", "WARN", "ERRO", "NONE"};

static const char* level_colors[] = {COLOR_YELLOW, // DEBUG
                                     COLOR_GREEN,  // INFO
                                     COLOR_YELLOW, // WARN
                                     COLOR_RED,    // ERROR
                                     COLOR_RESET}; // NONE

void log_message(
    hpp_log_level level, const char* file, int line, const char* func, const char* fmt, ...)
{
    if (level < global_log_level)
    {
        return;
    }
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "%s[%s]", level_colors[level], level_strings[level]);

    if (global_verbosity == 0)
    {
        fprintf(stderr, "%s: ", COLOR_RESET);
    }
    else if (global_verbosity == 1)
    {
        fprintf(stderr, "%s %s:%d: ", COLOR_RESET, file, line);
    }
    else
    {
        fprintf(stderr, "%s %s:%d (%s): ", COLOR_RESET, file, line, func);
    }

    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}

hpp_log_level logger_get_level(void)
{
    return global_log_level;
}

void logger_set_level(hpp_log_level level)
{
    global_log_level = level;
}

int logger_get_verbosity(void)
{
    return global_verbosity;
}

void logger_set_verbosity(int verbosity)
{
    if (verbosity < 0)
    {
        verbosity = 0;
    }
    if (verbosity > 2)
    {
        verbosity = 2;
    }
    global_verbosity = verbosity;
}
