#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "monitor.h"

int monitor_read(pid_t pid, proc_stats_t *out)
{
    char path[64];
    char buf[1024];

    if (pid <= 0 || out == NULL)
        return -1;

    /* Build the /proc stat file path */
    int len = snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);

    if (len < 0 || (size_t)len >= sizeof(path))
        return -1;

    /* Open process stat file */
    FILE *f = fopen(path, "r");

    if (f == NULL)
        return -1;

    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    int read_failed = ferror(f);

    fclose(f);

    if (read_failed || n == 0 || n >= sizeof(buf) - 1)
        return -1;

    buf[n] = '\0';

    /*
     * Process name is enclosed in parentheses.
     * It may contain spaces or parentheses, so find
     * the LAST closing parenthesis.
     */
    char *p = strrchr(buf, ')');

    if (p == NULL || p[1] != ' ')
        return -1;

    /*
     * Fields after the process name:
     *
     * 3  state
     * 4-13 fields to skip
     * 14 utime
     * 15 stime
     */
    char *saveptr = NULL;
    char *token = strtok_r(p + 2, " ", &saveptr);

    if (token == NULL || strlen(token) != 1)
        return -1;

    char state = token[0];

    /* Skip fields 4 through 13 */
    for (int i = 0; i < 10; i++)
    {
        token = strtok_r(NULL, " ", &saveptr);

        if (token == NULL)
            return -1;
    }

    /* Read utime (field 14) */
    token = strtok_r(NULL, " ", &saveptr);

    if (token == NULL)
        return -1;

    errno = 0;
    char *endptr;

    unsigned long ut = strtoul(token, &endptr, 10);

    if (errno != 0 || *endptr != '\0')
        return -1;

    /* Read stime (field 15) */
    token = strtok_r(NULL, " ", &saveptr);

    if (token == NULL)
        return -1;

    errno = 0;

    unsigned long st = strtoul(token, &endptr, 10);

    if (errno != 0 || *endptr != '\0')
        return -1;

    /* Prevent unsigned overflow */
    if (ULONG_MAX - ut < st)
        return -1;

    /* Get monotonic time */
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return -1;

    /* Get system clock ticks per second */
    long hz = sysconf(_SC_CLK_TCK);

    if (hz <= 0)
        return -1;

    unsigned long ticks = ut + st;

    /*
     * If PID changes, reset the previous baseline.
     */
    if (out->has_prev && out->pid != pid)
        out->has_prev = 0;

    double cpu = 0.0;

    if (out->has_prev)
    {
        double dt =
            (double)(now.tv_sec - out->prev_time.tv_sec) +
            (double)(now.tv_nsec - out->prev_time.tv_nsec) / 1e9;

        unsigned long delta = 0;

        if (ticks >= out->prev_ticks)
            delta = ticks - out->prev_ticks;

        if (dt > 0.0)
        {
            cpu = ((double)delta / (double)hz) / dt * 100.0;
        }
    }

    /* Update process statistics */
    out->cpu_percent = cpu;
    out->pid = pid;
    out->state = state;
    out->utime = ut;
    out->stime = st;

    /* Save current values for next measurement */
    out->prev_ticks = ticks;
    out->prev_time = now;
    out->has_prev = 1;

    return 0;
}