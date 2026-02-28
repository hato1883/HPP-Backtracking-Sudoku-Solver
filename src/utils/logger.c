#include "utils/logger.h"

#include <stdarg.h>
#include <stdio.h>

static hpp_log_level global_log_level = LOG_LEVEL;

static const char* level_strings[] = {"INFO", "WARN", "ERROR", "NONE"};

static const char* level_colors[] = {"\x1b[32m", // green
                                     "\x1b[33m", // yellow
                                     "\x1b[31m", // red
                                     "\x1b[0m"};

void log_message(
    hpp_log_level level, const char* file, int line, const char* func, const char* fmt, ...)
{
    if (level < global_log_level)
    {
        return;
    }
    va_list args;
    va_start(args, fmt);

    fprintf(stderr,
            "%s[%s]\t\x1b[0m %s:%d (%s): \t",
            level_colors[level],
            level_strings[level],
            file,
            line,
            func);

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
