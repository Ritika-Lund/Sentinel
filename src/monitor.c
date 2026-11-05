#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "monitor.h"

int monitor_read(pid_t pid, proc_stats_t *out)
{
    char path[64], buf[1024];
    snprintf(path, sizeof path, "/proc/%d/stat", (int)pid);

    FILE *f = fopen(path, "r");
    if (!f) return -1;
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    if (n == 0) return -1;
    buf[n] = '\0';

    /* process name is in parentheses and may contain spaces,
       so parse from the LAST ')' */
    char *p = strrchr(buf, ')');
    if (!p) return -1;

    char state;
    unsigned long ut, st;
    if (sscanf(p + 2,
               "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
               &state, &ut, &st) != 3)
        return -1;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    unsigned long ticks = ut + st;

    if (out->has_prev)
    {
        double dt = (now.tv_sec - out->prev_time.tv_sec) +
                    (now.tv_nsec - out->prev_time.tv_nsec) / 1e9;
        if (dt > 0)
            out->cpu_percent =
                ((double)(ticks - out->prev_ticks) /
                 sysconf(_SC_CLK_TCK)) / dt * 100.0;
    }
    else
        out->cpu_percent = 0.0;

    out->pid = pid;
    out->state = state;
    out->utime = ut;
    out->stime = st;
    out->prev_ticks = ticks;
    out->prev_time = now;
    out->has_prev = 1;
    return 0;
}
