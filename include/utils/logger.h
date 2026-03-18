/**
 * @file utils/logger.h
 * @brief Lightweight stderr logger with compile-time level filtering.
 */

#ifndef UTILS_LOGGER_H
#define UTILS_LOGGER_H

/* =========================================================================
 * Log Levels
 * ========================================================================= */

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

/* =========================================================================
 * Compile-Time Defaults
 * ========================================================================= */

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#ifndef LOG_VERBOSITY
#define LOG_VERBOSITY 0
#endif

/* =========================================================================
 * Core API
 * ========================================================================= */

/**
 * @brief Emit one log message.
 *
 * Prefer the convenience macros (`LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`,
 * `LOG_ERROR`) so call-site metadata is added automatically.
 *
 * @note Prefer macro wrappers to auto-capture file/line/function metadata.
 * @pre `fmt != NULL`.
 * @post One formatted line is written to stderr when level is enabled.
 *
 * @param level Log level.
 * @param file Source file path.
 * @param line Source line number.
 * @param func Source function name.
 * @param fmt `printf`-style format string.
 * @param ... Format arguments.
 */
void log_message(
    hpp_log_level level, const char* file, int line, const char* func, const char* fmt, ...);

/* =========================================================================
 * Convenience Macros
 * ========================================================================= */

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

/* =========================================================================
 * Runtime Controls
 * ========================================================================= */

/**
 * @brief Get current global log level.
 *
 * @pre None.
 * @post Logger state is unchanged.
 *
 * @return Current runtime log level.
 */
hpp_log_level logger_get_level(void);

/**
 * @brief Set global log level at runtime.
 *
 * @note Runtime level can further restrict compile-time-enabled logs.
 * @pre `level` is one of `hpp_log_level` values.
 * @post Subsequent log emission uses the new runtime threshold.
 *
 * @param level New runtime log level.
 *
 * @par Example
 * @code{.c}
 * logger_set_level(LOG_LEVEL_WARN);
 * LOG_INFO("this message is suppressed at runtime");
 * @endcode
 */
void logger_set_level(hpp_log_level level);

/**
 * @brief Get current verbosity level (`0..2`).
 *
 * @pre None.
 * @post Logger state is unchanged.
 *
 * @return Current verbosity level.
 */
int logger_get_verbosity(void);

/**
 * @brief Set verbosity level (`0..2`).
 *
 * Higher verbosity includes file/line/function metadata in output.
 *
 * @pre Any integer is accepted; implementation clamps to valid range.
 * @post Runtime verbosity is set to a value in `[0, 2]`.
 *
 * @param verbosity New verbosity level.
 */
void logger_set_verbosity(int verbosity);

#endif /* UTILS_LOGGER_H */