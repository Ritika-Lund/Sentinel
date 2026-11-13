#ifndef MONITOR_H
#define MONITOR_H

#include <sys/types.h>
#include <time.h>

typedef struct
{
    pid_t pid;
    char state;
    unsigned long utime;
    unsigned long stime;
    double cpu_percent;
    unsigned long prev_ticks;
    struct timespec prev_time;
    int has_prev;
} proc_stats_t;

/*
 * Initialize the structure to zero before first use.
 * Returns 0 on success, -1 if the process is gone or unreadable.
 */
int monitor_read(pid_t pid, proc_stats_t *out);

#endif