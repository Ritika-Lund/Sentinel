#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include "logger.h"

static FILE *g_fp = NULL;                 /* protected by g_lock */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

int log_init(const char *path)
{
    int rc = 0;

    if (path == NULL)
        return -1;

    pthread_mutex_lock(&g_lock);
    if (g_fp == NULL)
    {
        g_fp = fopen(path, "a");
        if (g_fp == NULL)
            rc = -1;
    }
    /* already open: keep it, do not leak a second FILE* */
    pthread_mutex_unlock(&g_lock);

    return rc;                            /* no access to g_fp outside the lock */
}

void log_event(const char *level, const char *fmt, ...)
{
    char ts[32];
    time_t now = time(NULL);
    struct tm tmv;

    if (level == NULL || fmt == NULL)
        return;

    if (localtime_r(&now, &tmv) == NULL ||
        strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tmv) == 0)
        snprintf(ts, sizeof ts, "unknown-time");

    pthread_mutex_lock(&g_lock);
    if (g_fp != NULL)
    {
        va_list ap;

        fprintf(g_fp, "[%s] [%s] ", ts, level);
        va_start(ap, fmt);
        vfprintf(g_fp, fmt, ap);
        va_end(ap);
        fputc('\n', g_fp);
        fflush(g_fp);
    }
    pthread_mutex_unlock(&g_lock);
}

void log_close(void)
{
    pthread_mutex_lock(&g_lock);
    if (g_fp != NULL)
    {
        fclose(g_fp);
        g_fp = NULL;
    }
    pthread_mutex_unlock(&g_lock);
}
