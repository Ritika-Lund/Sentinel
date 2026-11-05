#ifndef MONITOR_H
#define MONITOR_H

#include <sys/types.h>
#include <time.h>

typedef struct
{
    pid_t pid;
    char state;                /* R, S, D, T, Z ... from /proc */
    unsigned long utime;       /* clock ticks */
    unsigned long stime;       /* clock ticks */
    double cpu_percent;        /* since previous call */
    unsigned long prev_ticks;  /* internal */
    struct timespec prev_time; /* internal */
    int has_prev;              /* internal */
} proc_stats_t;

/* memset the struct to 0 before first use.
   Returns 0 on success, -1 if the process is gone or unreadable. */
int monitor_read(pid_t pid, proc_stats_t *out);

#endif
