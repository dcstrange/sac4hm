/*
    This Timer Utilities is applied to count the **micro-second** of a process block
*/
#ifndef _TIMER_UTILS_H_
#define _TIMER_UTILS_H_

#include <stdio.h>
#include <sys/time.h>

struct timeval;

typedef __useconds_t microsecond_t;

static inline void 
Lap(struct timeval* tv)
{
    gettimeofday(tv, NULL);
}

static inline microsecond_t 
TimerInterval_useconds(struct timeval* tv_start, struct timeval* tv_stop)
{
    return (tv_stop->tv_sec-tv_start->tv_sec)*1000000 + (tv_stop->tv_usec-tv_start->tv_usec);
}

static inline double 
TimerInterval_seconds(struct timeval* tv_start, struct timeval* tv_stop)
{
    return (tv_stop->tv_sec-tv_start->tv_sec) + (tv_stop->tv_usec-tv_start->tv_usec)/1000000.0;
}

inline double 
Mirco2Sec(microsecond_t msecond)
{
    return msecond/1000000.0;
}

inline double 
Mirco2Milli(microsecond_t msecond)
{
    return msecond/1000.0;
}

#endif // _TIMER_UTILS_H_
