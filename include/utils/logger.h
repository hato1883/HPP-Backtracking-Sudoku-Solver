#ifndef UTILS_LOGGER_H
#define UTILS_LOGGER_H

/* ===============================
   Log Levels
   =============================== */

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_NONE 4

typedef enum
{
    DEBUG = LOG_LEVEL_DEBUG,
    INFO  = LOG_LEVEL_INFO,
    WARN  = LOG_LEVEL_WARN,
    ERROR = LOG_LEVEL_ERROR,
    NONE  = LOG_LEVEL_NONE,
} hpp_log_level;

/* ===============================
   Configuration
   =============================== */

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#ifndef LOG_VERBOSITY
#define LOG_VERBOSITY 0
#endif

/**
 * Logs information to the terminal
 *
 * @note Use the macros such as @code LOG_WARN provided instead
 *
 * @param level logging level to use {LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN,
 * LOG_LEVEL_ERROR, LOG_LEVEL_NONE}
 * @param file source file in which the log was invoked from
 * @param line line in the source file
 * @param func function in the source file
`[INFO]   src/main.c:17:  and bitmaks:`
 * @param fmt printf format string
 * @param ... Varargs of objects to insert into the string
 */
void log_message(
    hpp_log_level level, const char* file, int line, const char* func, const char* fmt, ...);

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...)                                                                        \
    log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(fmt, ...)                                                                         \
    log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
#define LOG_WARN(fmt, ...)                                                                         \
    log_message(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_WARN(fmt, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(fmt, ...)                                                                        \
    log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...)
#endif

hpp_log_level logger_get_level(void);

void logger_set_level(hpp_log_level level);

/**
 * Get the current verbosity level
 *
 * @return Current verbosity level (0, 1, or 2)
 */
int logger_get_verbosity(void);

/**
 * Set the verbosity level
 *
 * @param verbosity Verbosity level:
 *   0: Only log level (e.g., [INFO]: message)
 *   1: Log level + file + line (e.g., [INFO]   file.c:17: message)
 *   2: Log level + file + line + function (e.g., [INFO]   file.c:17 (func): message)
 */
void logger_set_verbosity(int verbosity);

#endif