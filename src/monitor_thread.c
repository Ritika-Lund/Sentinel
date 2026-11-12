#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include "monitor_thread.h"
#include "logger.h"

#define MON_MAX 64

static pthread_t g_thread;
static atomic_bool g_started = false;
static atomic_bool g_stop = false;

static WorkerProcess *s_workers = NULL;
static int g_count = 0;
static pthread_mutex_t *g_lock = NULL;

/* Per-worker data. Only the monitor thread writes it; g_stats_lock
   protects it for readers such as monitor_get_stats(). */
static proc_stats_t g_stats[MON_MAX];
static pid_t g_stats_pid[MON_MAX];
static WorkerState g_last_state[MON_MAX];
static char g_last_proc_state[MON_MAX];
static int g_unreadable[MON_MAX];
static pthread_mutex_t g_stats_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *state_name(WorkerState s)
{
    switch (s)
    {
        case WORKER_NOT_STARTED: return "NOT_STARTED";
        case WORKER_RUNNING:     return "RUNNING";
        case WORKER_PAUSED:      return "PAUSED";
        case WORKER_CRASHED:     return "CRASHED";
        case WORKER_RESTARTING:  return "RESTARTING";
        case WORKER_STOPPED:     return "STOPPED";
        case WORKER_FAILED:      return "FAILED";
        default:                 return "UNKNOWN";
    }
}

static void *monitor_loop(void *arg)
{
    (void)arg;
    unsigned long cycle = 0;

    while (!atomic_load(&g_stop))
    {
        pid_t pids[MON_MAX];
        int ids[MON_MAX];
        WorkerState states[MON_MAX];

        /* 1. copy what we need while holding the lock, then release it */
        pthread_mutex_lock(g_lock);
        for (int i = 0; i < g_count; i++)
        {
            pids[i] = s_workers[i].pid;
            ids[i] = s_workers[i].worker_id;
            states[i] = s_workers[i].state;
        }
        pthread_mutex_unlock(g_lock);

        /* 2. read /proc without holding the lock */
        for (int i = 0; i < g_count; i++)
        {
            if (states[i] != g_last_state[i])
            {
                log_event("INFO", "Worker %d state: %s -> %s", ids[i],
                          state_name(g_last_state[i]), state_name(states[i]));
                g_last_state[i] = states[i];
            }

            if (pids[i] != g_stats_pid[i])      /* new or restarted worker */
            {
                pthread_mutex_lock(&g_stats_lock);
                memset(&g_stats[i], 0, sizeof g_stats[i]);
                g_stats_pid[i] = pids[i];
                pthread_mutex_unlock(&g_stats_lock);
                g_last_proc_state[i] = 0;
                g_unreadable[i] = 0;
            }

            if (pids[i] <= 0)
                continue;
            if (states[i] != WORKER_RUNNING && states[i] != WORKER_PAUSED)
                continue;

            proc_stats_t s = g_stats[i];
            if (monitor_read(pids[i], &s) == 0)
            {
                pthread_mutex_lock(&g_stats_lock);
                g_stats[i] = s;
                pthread_mutex_unlock(&g_stats_lock);
                g_unreadable[i] = 0;

                if (s.state != g_last_proc_state[i])
                {
                    if (s.state == 'Z')
                        log_event("WARN", "Worker %d (pid %d) is a zombie",
                                  ids[i], (int)pids[i]);
                    g_last_proc_state[i] = s.state;
                }
                if (cycle % 5 == 0)
                    log_event("INFO", "Worker %d pid=%d state=%c cpu=%.1f%%",
                              ids[i], (int)pids[i], s.state, s.cpu_percent);
            }
            else if (!g_unreadable[i])
            {
                log_event("WARN", "Worker %d: pid %d not readable in /proc",
                          ids[i], (int)pids[i]);
                g_unreadable[i] = 1;
            }
        }

        cycle++;

        /* sleep 1 second in small slices so monitor_stop() is quick */
        for (int k = 0; k < 10 && !atomic_load(&g_stop); k++)
            usleep(100000);
    }
    return NULL;
}

int monitor_start(WorkerProcess *workers, int count, pthread_mutex_t *lock)
{
    if (atomic_load(&g_started) || !workers || !lock ||
        count < 1 || count > MON_MAX)
        return -1;

    s_workers = workers;
    g_count = count;
    g_lock = lock;
    atomic_store(&g_stop, false);

    memset(g_stats, 0, sizeof g_stats);
    memset(g_stats_pid, 0, sizeof g_stats_pid);
    memset(g_last_state, 0, sizeof g_last_state);   /* 0 = NOT_STARTED */
    memset(g_last_proc_state, 0, sizeof g_last_proc_state);
    memset(g_unreadable, 0, sizeof g_unreadable);

    if (pthread_create(&g_thread, NULL, monitor_loop, NULL) != 0)
        return -1;
    atomic_store(&g_started, true);
    log_event("INFO", "Monitor thread started (%d workers)", count);
    return 0;
}

void monitor_stop(void)
{
    if (!atomic_exchange(&g_started, false))
        return;
    atomic_store(&g_stop, true);
    pthread_join(g_thread, NULL);
    log_event("INFO", "Monitor thread stopped");
}

int monitor_get_stats(int index, proc_stats_t *out)
{
    int rc = -1;
    if (index < 0 || index >= MON_MAX || !out)
        return -1;

    pthread_mutex_lock(&g_stats_lock);
    if (g_stats_pid[index] > 0 && g_stats[index].has_prev)
    {
        *out = g_stats[index];
        rc = 0;
    }
    pthread_mutex_unlock(&g_stats_lock);
    return rc;
}
