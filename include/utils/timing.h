/**
 * @file utils/timing.h
 * @brief Cross-platform monotonic timing helpers.
 */

#ifndef UTILS_TIMING_H
#define UTILS_TIMING_H

#include <stdint.h>

enum
{
    TIMER_NS_IN_S = 1000000000ULL,
    TIMER_US_IN_S = 1000000ULL,
    TIMER_MS_IN_S = 1000ULL,
};

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>

typedef struct
{
    LARGE_INTEGER start;
    LARGE_INTEGER end;
} hpp_timer;

/**
 * @brief Start timer measurement.
 *
 * @note Uses platform monotonic clock source.
 * @pre `timer != NULL`.
 * @post `timer->start` stores a fresh timestamp.
 *
 * @param timer Timer instance to start.
 */
static inline void timer_start(hpp_timer* timer)
{
    QueryPerformanceCounter(&timer->start);
}

/**
 * @brief Stop timer measurement.
 *
 * @pre `timer != NULL` and `timer_start` was called previously.
 * @post `timer->end` stores a fresh timestamp.
 *
 * @param timer Timer instance to stop.
 */
static inline void timer_stop(hpp_timer* timer)
{
    QueryPerformanceCounter(&timer->end);
}

/**
 * @brief Return elapsed time in nanoseconds.
 *
 * @pre `timer != NULL` and both start/end fields are initialized.
 * @post Timer object is unchanged.
 *
 * @param timer Timer instance.
 * @return Elapsed time in nanoseconds.
 */
static inline uint64_t timer_ns(const hpp_timer* timer)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (uint64_t)((timer->end.QuadPart - timer->start.QuadPart) * TIMER_NS_IN_S /
                      freq.QuadPart);
}

#elif defined(__APPLE__)

#include <mach/mach_time.h>

typedef struct
{
    uint64_t start;
    uint64_t end;
} hpp_timer;

/**
 * @brief Start timer measurement.
 *
 * @note Uses platform monotonic clock source.
 * @pre `timer != NULL`.
 * @post `timer->start` stores a fresh timestamp.
 *
 * @param timer Timer instance to start.
 */
static inline void timer_start(hpp_timer* timer)
{
    timer->start = mach_absolute_time();
}

/**
 * @brief Stop timer measurement.
 *
 * @pre `timer != NULL` and `timer_start` was called previously.
 * @post `timer->end` stores a fresh timestamp.
 *
 * @param timer Timer instance to stop.
 */
static inline void timer_stop(hpp_timer* timer)
{
    timer->end = mach_absolute_time();
}

/**
 * @brief Return elapsed time in nanoseconds.
 *
 * @pre `timer != NULL` and both start/end fields are initialized.
 * @post Timer object is unchanged.
 *
 * @param timer Timer instance.
 * @return Elapsed time in nanoseconds.
 */
static inline uint64_t timer_ns(const hpp_timer* timer)
{
    // Cached conversion ratio from mach absolute ticks to nanoseconds.
    static mach_timebase_info_data_t info = {0, 0};
    if (info.denom == 0)
    {
        mach_timebase_info(&info);
    }

    return (timer->end - timer->start) * info.numer / info.denom;
}

#else /* POSIX (Linux, FreeBSD, etc.) */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <time.h>

typedef struct
{
    struct timespec start;
    struct timespec end;
} hpp_timer;

#ifdef CLOCK_MONOTONIC_RAW
#define MONOTONIC_CLOCK CLOCK_MONOTONIC_RAW
#else
#define MONOTONIC_CLOCK CLOCK_MONOTONIC
#endif

/**
 * @brief Start timer measurement.
 *
 * @note Uses platform monotonic clock source.
 * @pre `timer != NULL`.
 * @post `timer->start` stores a fresh timestamp.
 *
 * @param timer Timer instance to start.
 */
static inline void timer_start(hpp_timer* timer)
{
    clock_gettime(MONOTONIC_CLOCK, &timer->start);
}

/**
 * @brief Stop timer measurement.
 *
 * @pre `timer != NULL` and `timer_start` was called previously.
 * @post `timer->end` stores a fresh timestamp.
 *
 * @param timer Timer instance to stop.
 */
static inline void timer_stop(hpp_timer* timer)
{
    clock_gettime(MONOTONIC_CLOCK, &timer->end);
}

/**
 * @brief Return elapsed time in nanoseconds.
 *
 * @pre `timer != NULL` and both start/end fields are initialized.
 * @post Timer object is unchanged.
 *
 * @param timer Timer instance.
 * @return Elapsed time in nanoseconds.
 */
static inline uint64_t timer_ns(const hpp_timer* timer)
{
    return ((timer->end.tv_sec - timer->start.tv_sec) * TIMER_NS_IN_S) +
           (timer->end.tv_nsec - timer->start.tv_nsec);
}

#endif

/**
 * @brief Return elapsed time in microseconds.
 *
 * @note Derived from `timer_ns`.
 * @pre `timer != NULL`.
 * @post Timer object is unchanged.
 *
 * @param timer Timer instance.
 * @return Elapsed time in microseconds.
 */
static inline double timer_us(const hpp_timer* timer)
{
    return (double)timer_ns(timer) / TIMER_MS_IN_S;
}

/**
 * @brief Return elapsed time in milliseconds.
 *
 * @note Derived from `timer_ns`.
 * @pre `timer != NULL`.
 * @post Timer object is unchanged.
 *
 * @param timer Timer instance.
 * @return Elapsed time in milliseconds.
 */
static inline double timer_ms(const hpp_timer* timer)
{
    return (double)timer_ns(timer) / TIMER_US_IN_S;
}

/**
 * @brief Return elapsed time in seconds.
 *
 * @note Derived from `timer_ns`.
 * @pre `timer != NULL`.
 * @post Timer object is unchanged.
 *
 * @param timer Timer instance.
 * @return Elapsed time in seconds.
 *
 * @par Example
 * @code{.c}
 * hpp_timer t;
 * timer_start(&t);
 * // work
 * timer_stop(&t);
 * printf("elapsed: %.6f s\n", timer_s(&t));
 * @endcode
 */
static inline double timer_s(const hpp_timer* timer)
{
    return (double)timer_ns(timer) / TIMER_NS_IN_S;
}

#endif /* UTILS_TIMING_H */