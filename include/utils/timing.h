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
    LARGE_INTEGER start, end;
} hpp_timer;

static inline uint64_t timer_ns(const hpp_timer* timer);

static inline void timer_start(hpp_timer* timer)
{
    QueryPerformanceCounter(&timer->start);
}

static inline void timer_stop(hpp_timer* timer)
{
    QueryPerformanceCounter(&timer->end);
}

/**
 * Time taken in nano secounds, this is the most precise messurment
 * @return Time elapsed in nano secounds (1e-9 secounds)
 */
static inline uint64_t timer_ns(hpp_timer* timer)
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
    uint64_t start, end;
} hpp_timer;

static inline uint64_t timer_ns(const hpp_timer* timer);

static inline void timer_start(hpp_timer* timer)
{
    timer->start = mach_absolute_time();
}

static inline void timer_stop(hpp_timer* timer)
{
    timer->end = mach_absolute_time();
}

/**
 * Time taken in nano secounds, this is the most precise messurment
 * @return Time elapsed in nano secounds (1e-9 secounds)
 */
static inline uint64_t timer_ns(hpp_timer* timer)
{
    static mach_timebase_info_data_t info = {0, 0};
    if (info.denom == 0)
        mach_timebase_info(&info);
    return (timer->end - timer->start) * info.numer / info.denom;
}

#else // POSIX (Linux, FreeBSD, etc)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <time.h>

typedef struct
{
    struct timespec start, end;
} hpp_timer;

static inline uint64_t timer_ns(const hpp_timer* timer);

#ifdef CLOCK_MONOTONIC_RAW
#define MONOTONIC_CLOCK CLOCK_MONOTONIC_RAW
#else
#define MONOTONIC_CLOCK CLOCK_MONOTONIC
#endif

static inline void timer_start(hpp_timer* timer)
{
    clock_gettime(MONOTONIC_CLOCK, &timer->start);
}

static inline void timer_stop(hpp_timer* timer)
{
    clock_gettime(MONOTONIC_CLOCK, &timer->end);
}

/**
 * Time taken in nano secounds, this is the most precise messurment
 * @return Time elapsed in nano secounds (1e-9 secounds)
 */
static inline uint64_t timer_ns(const hpp_timer* timer)
{
    return ((timer->end.tv_sec - timer->start.tv_sec) * TIMER_NS_IN_S) +
           (timer->end.tv_nsec - timer->start.tv_nsec);
}

#endif

/**
 * Time taken in microsecounds, this is messurment may suffer from floating point precision
 * This method simply divides time recived from `timer_ns` with `TIMER_MS_IN_S`
 * @return Time elapsed in microsecounds (1e-6 secounds)
 */
static inline double timer_us(const hpp_timer* timer)
{
    return (double)timer_ns(timer) / TIMER_MS_IN_S;
}

/**
 * Time taken in milisecounds, this is messurment may suffer from floating point precision
 * This method simply divides time recived from `timer_ns` with `TIMER_US_IN_S`
 * @return Time elapsed in milisecounds (1e-3 secounds)
 */
static inline double timer_ms(const hpp_timer* timer)
{
    return (double)timer_ns(timer) / TIMER_US_IN_S;
}

/**
 * Time taken in secounds, this is messurment may suffer from floating point precision
 * This method simply divides time recived from `timer_ns` with `TIMER_NS_IN_S`
 * @return Time elapsed in secounds
 */
static inline double timer_s(const hpp_timer* timer)
{
    return (double)timer_ns(timer) / TIMER_NS_IN_S;
}

#endif