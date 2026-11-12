#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "monitor.h"

int monitor_read(pid_t pid, proc_stats_t *out)
{
    char path[64], buf[1024];

    if (pid <= 0 || out == NULL)
        return -1;

    int len = snprintf(path, sizeof path, "/proc/%d/stat", (int)pid);
    if (len < 0 || (size_t)len >= sizeof path)
        return -1;

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    int read_failed = ferror(f);
    fclose(f);
    if (read_failed || n == 0)
        return -1;
    buf[n] = '\0';

    /* process name is in parentheses and may contain spaces,
       so parse from the LAST ')' */
    char *p = strrchr(buf, ')');
    if (!p || p[1] != ' ')
        return -1;

    char state;
    unsigned long ut, st;
    if (sscanf(p + 2,
               "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
               &state, &ut, &st) != 3)
        return -1;

    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return -1;

    long hz = sysconf(_SC_CLK_TCK);
    if (hz <= 0)
        return -1;

    unsigned long ticks = ut + st;

    /* a different pid means a different process: start a new baseline */
    if (out->has_prev && out->pid != pid)
        out->has_prev = 0;

    double cpu = 0.0;
    if (out->has_prev)
    {
        double dt = (double)(now.tv_sec - out->prev_time.tv_sec) +
                    (double)(now.tv_nsec - out->prev_time.tv_nsec) / 1e9;
        /* guard against counter going backwards (unsigned underflow) */
        unsigned long delta = (ticks >= out->prev_ticks)
                                  ? ticks - out->prev_ticks : 0;
        if (dt > 0.0)
            cpu = ((double)delta / (double)hz) / dt * 100.0;
    }

    out->cpu_percent = cpu;
    out->pid = pid;
    out->state = state;
    out->utime = ut;
    out->stime = st;
    out->prev_ticks = ticks;
    out->prev_time = now;
    out->has_prev = 1;
    return 0;
}
