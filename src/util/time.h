/**
 * @file time.h
 * time related methods
 */
#ifndef MPXMANAGER_TIME_H_
#define MPXMANAGER_TIME_H_
#include <time.h>
#include <sys/time.h>
/**
 * Returns a monotonically increasing number that servers the time in ms.
 * @return the current time (ms)
 */
static inline unsigned int getTime() {
    struct timeval start;
    gettimeofday(&start, NULL);
    return start.tv_sec * 1000 + start.tv_usec * 1e-3;
}
#endif
