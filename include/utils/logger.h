#ifndef UTILS_LOGGER_H
#define UTILS_LOGGER_H

/* ===============================
   Log Levels
   =============================== */

typedef enum
{
    LOG_LEVEL_INFO = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_NONE
} hpp_log_level;

/* ===============================
   Configuration
   =============================== */

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

/**
 * Logs information to the terminal
 *
 * @note Use the macros such as @code LOG_WARN provided instead
 *
 * @param level logging level to use {LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR,
 * LOG_LEVEL_NONE}
 * @param file source file in which the log was invoked from
 * @param line line in the source file
 * @param func function in the source file
 * @param fmt printf format string
 * @param ... Varargs of objects to insert into the string
 */
void log_message(
    hpp_log_level level, const char* file, int line, const char* func, const char* fmt, ...);

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

#endif