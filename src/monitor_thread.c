#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

#include "monitor_thread.h"
#include "logger.h"

#define MON_MAX MAX_WORKERS

static pthread_t g_thread;

static atomic_bool g_started = false;
static atomic_bool g_stop = false;

static pthread_mutex_t g_lifecycle_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_stats_lock = PTHREAD_MUTEX_INITIALIZER;

static WorkerProcess *s_workers = NULL;
static int g_count = 0;
static pthread_mutex_t *g_lock = NULL;

/* Per-worker monitoring data */
static proc_stats_t g_stats[MON_MAX];
static pid_t g_stats_pid[MON_MAX];

static WorkerState g_last_state[MON_MAX];
static char g_last_proc_state[MON_MAX];

static int g_unreadable[MON_MAX];
static bool g_state_seen[MON_MAX];

/* Convert worker state to readable text */
static const char *state_name(WorkerState s)
{
    switch (s)
    {
        case WORKER_NOT_STARTED:
            return "NOT_STARTED";

        case WORKER_RUNNING:
            return "RUNNING";

        case WORKER_PAUSED:
            return "PAUSED";

        case WORKER_CRASHED:
            return "CRASHED";

        case WORKER_RESTARTING:
            return "RESTARTING";

        case WORKER_STOPPED:
            return "STOPPED";

        case WORKER_FAILED:
            return "FAILED";

        default:
            return "UNKNOWN";
    }
}

/* Background monitoring thread */
static void *monitor_loop(void *arg)
{
    (void)arg;

    while (!atomic_load(&g_stop))
    {
        pid_t pids[MON_MAX];
        WorkerState states[MON_MAX];

        /*
         * Copy worker information while holding the
         * process registry mutex.
         */
        pthread_mutex_lock(g_lock);

        for (int i = 0; i < g_count; i++)
        {
            pids[i] = s_workers[i].pid;
            states[i] = s_workers[i].state;
        }

        pthread_mutex_unlock(g_lock);

        /*
         * Monitor each worker.
         */
        for (int i = 0; i < g_count; i++)
        {
            if (atomic_load(&g_stop))
                break;

            pid_t pid = pids[i];
            WorkerState worker_state = states[i];

            /* Log worker state changes */
            pthread_mutex_lock(&g_stats_lock);

            if (!g_state_seen[i] ||
                g_last_state[i] != worker_state)
            {
                log_event(
                    "INFO",
                    "Worker %d state changed: %s",
                    i + 1,
                    state_name(worker_state)
                );

                g_last_state[i] = worker_state;
                g_state_seen[i] = true;
            }

            pthread_mutex_unlock(&g_stats_lock);

            /* No running process to inspect */
            if (pid <= 0)
            {
                pthread_mutex_lock(&g_stats_lock);

                g_stats_pid[i] = 0;
                memset(&g_stats[i], 0, sizeof(g_stats[i]));
                g_last_proc_state[i] = '\0';
                g_unreadable[i] = 0;

                pthread_mutex_unlock(&g_stats_lock);

                continue;
            }

            /*
             * Read process statistics.
             * Keep the stats mutex while updating the
             * shared statistics structure.
             */
            pthread_mutex_lock(&g_stats_lock);

            /* Reset baseline if the worker has a new PID */
            if (g_stats_pid[i] != pid)
            {
                memset(&g_stats[i], 0, sizeof(g_stats[i]));
                g_stats_pid[i] = pid;
                g_last_proc_state[i] = '\0';
                g_unreadable[i] = 0;
            }

            int result = monitor_read(pid, &g_stats[i]);

            if (result == 0)
            {
                g_unreadable[i] = 0;

                /* Log process state changes */
                if (g_last_proc_state[i] != g_stats[i].state)
                {
                    log_event(
                        "INFO",
                        "Worker %d (PID %d) process state: %c",
                        i + 1,
                        (int)pid,
                        g_stats[i].state
                    );

                    g_last_proc_state[i] = g_stats[i].state;
                }
            }
            else
            {
                /*
                 * Avoid logging the same unreadable
                 * process every monitoring cycle.
                 */
                if (!g_unreadable[i])
                {
                    log_event(
                        "WARNING",
                        "Unable to read /proc statistics for Worker %d (PID %d)",
                        i + 1,
                        (int)pid
                    );
                }

                g_unreadable[i] = 1;
            }

            pthread_mutex_unlock(&g_stats_lock);
        }

        /* Check workers once per second */
        for (int j = 0; j < 10; j++)
        {
            if (atomic_load(&g_stop))
                break;

            usleep(100000);
        }
    }

    return NULL;
}

/* Start monitoring */
int monitor_start(
    WorkerProcess *workers,
    int count,
    pthread_mutex_t *lock
)
{
    pthread_mutex_lock(&g_lifecycle_lock);

    if (atomic_load(&g_started) ||
        workers == NULL ||
        lock == NULL ||
        count < 1 ||
        count > MON_MAX)
    {
        pthread_mutex_unlock(&g_lifecycle_lock);
        return -1;
    }

    s_workers = workers;
    g_count = count;
    g_lock = lock;

    atomic_store(&g_stop, false);

    /* Reset monitoring data */
    pthread_mutex_lock(&g_stats_lock);

    memset(g_stats, 0, sizeof(g_stats));
    memset(g_stats_pid, 0, sizeof(g_stats_pid));
    memset(g_last_state, 0, sizeof(g_last_state));
    memset(g_last_proc_state, 0, sizeof(g_last_proc_state));
    memset(g_unreadable, 0, sizeof(g_unreadable));
    memset(g_state_seen, 0, sizeof(g_state_seen));

    pthread_mutex_unlock(&g_stats_lock);

    /* Create monitoring thread */
    if (pthread_create(&g_thread, NULL, monitor_loop, NULL) != 0)
    {
        s_workers = NULL;
        g_count = 0;
        g_lock = NULL;

        pthread_mutex_unlock(&g_lifecycle_lock);
        return -1;
    }

    atomic_store(&g_started, true);

    log_event(
        "INFO",
        "Monitor thread started (%d workers)",
        count
    );

    pthread_mutex_unlock(&g_lifecycle_lock);

    return 0;
}

/* Stop monitoring */
void monitor_stop(void)
{
    pthread_mutex_lock(&g_lifecycle_lock);

    if (!atomic_load(&g_started))
    {
        pthread_mutex_unlock(&g_lifecycle_lock);
        return;
    }

    atomic_store(&g_stop, true);

    /* Wait for monitoring thread to finish */
    pthread_join(g_thread, NULL);

    atomic_store(&g_started, false);

    log_event("INFO", "Monitor thread stopped");

    s_workers = NULL;
    g_count = 0;
    g_lock = NULL;

    pthread_mutex_unlock(&g_lifecycle_lock);
}

/* Get latest statistics for a worker */
int monitor_get_stats(int index, proc_stats_t *out)
{
    if (out == NULL)
        return -1;

    pthread_mutex_lock(&g_lifecycle_lock);

    if (index < 0 || index >= g_count)
    {
        pthread_mutex_unlock(&g_lifecycle_lock);
        return -1;
    }

    pthread_mutex_lock(&g_stats_lock);

    int result = -1;

    if (g_stats_pid[index] > 0 &&
        g_stats[index].has_prev)
    {
        *out = g_stats[index];
        result = 0;
    }

    pthread_mutex_unlock(&g_stats_lock);
    pthread_mutex_unlock(&g_lifecycle_lock);

    return result;
}