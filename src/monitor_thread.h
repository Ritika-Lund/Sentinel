#ifndef MONITOR_THREAD_H
#define MONITOR_THREAD_H

#include <pthread.h>
#include "process.h"
#include "monitor.h"

/* Starts a background thread that checks every worker once per second.
   workers/count: the worker array. lock: the mutex that protects it.
   Returns 0 on success, -1 on error. */
int monitor_start(
    WorkerProcess *workers,
    int count,
    pthread_mutex_t *lock
);

/* Stops the thread and waits for it to finish. Safe to call twice. */
void monitor_stop(void);

/* Latest /proc stats for worker number index (0-based).
   Returns 0 on success, -1 if there is no data yet. */
int monitor_get_stats(int index, proc_stats_t *out);

#endif