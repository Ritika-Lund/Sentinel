#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include "logger.h"

static FILE *g_fp = NULL;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

int log_init(const char *path)
{
    pthread_mutex_lock(&g_lock);
    g_fp = fopen(path, "a");
    pthread_mutex_unlock(&g_lock);
    return g_fp ? 0 : -1;
}

void log_event(const char *level, const char *fmt, ...)
{
    char ts[32];
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tmv);

    pthread_mutex_lock(&g_lock);
    if (g_fp)
    {
        fprintf(g_fp, "[%s] [%s] ", ts, level);
        va_list ap;
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
    if (g_fp) { fclose(g_fp); g_fp = NULL; }
    pthread_mutex_unlock(&g_lock);
}
