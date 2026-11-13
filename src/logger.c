#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

#include "logger.h"

/* Shared log file and mutex */
static FILE *g_fp = NULL;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;


/* Initialize logger */
int log_init(const char *path)
{
    if (path == NULL)
        return -1;

    pthread_mutex_lock(&g_lock);

    int rc = 0;

    if (g_fp == NULL)
    {
        g_fp = fopen(path, "a");

        if (g_fp == NULL)
            rc = -1;
    }

    pthread_mutex_unlock(&g_lock);

    return rc;
}


/* Write an event to the log file */
void log_event(const char *level, const char *fmt, ...)
{
    if (level == NULL || fmt == NULL)
        return;

    char timestamp[32];

    time_t now = time(NULL);
    struct tm tmv;

    if (localtime_r(&now, &tmv) == NULL ||
        strftime(timestamp, sizeof(timestamp),
                 "%Y-%m-%d %H:%M:%S", &tmv) == 0)
    {
        snprintf(timestamp, sizeof(timestamp), "unknown-time");
    }

    pthread_mutex_lock(&g_lock);

    if (g_fp != NULL)
    {
        fprintf(g_fp, "[%s] [%s] ", timestamp, level);

        va_list args;
        va_start(args, fmt);

        vfprintf(g_fp, fmt, args);

        va_end(args);

        fputc('\n', g_fp);

        /* Ensure log data is written immediately */
        fflush(g_fp);
    }

    pthread_mutex_unlock(&g_lock);
}


/* Close logger */
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