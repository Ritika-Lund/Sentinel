#include <stdio.h>
#include <pthread.h>

#include "recovery.h"


int recovery_check_and_restart(WorkerProcess *worker,
                               const char *worker_path,
                               int max_restarts)
{
    if (worker == NULL)
    {
        return -1;
    }

    // Check whether worker has exited
    int result = process_check_exit(worker);

    if (result == 0)
    {
        return 0;
    }

    if (result == -1)
    {
        return -1;
    }

    pthread_mutex_lock(&g_workers_lock);

    // Do not restart a worker that stopped normally
    if (worker->state != WORKER_CRASHED)
    {
        pthread_mutex_unlock(&g_workers_lock);
        return 0;
    }

    int worker_id = worker->worker_id;

    // Check maximum restart limit
    if (worker->restart_count >= max_restarts)
    {
        worker->state = WORKER_FAILED;

        pthread_mutex_unlock(&g_workers_lock);

        printf("[RECOVERY] Worker %d reached maximum restart limit.\n",
               worker_id);

        printf("[STATE] Worker %d: FAILED\n", worker_id);

        return 2;
    }

    worker->state = WORKER_RESTARTING;
    worker->restart_count++;

    int restart_attempt = worker->restart_count;

    pthread_mutex_unlock(&g_workers_lock);

    printf("[RECOVERY] Restarting Worker %d...\n",
           worker_id);

    printf("[STATE] Worker %d: RESTARTING\n",
           worker_id);

    // Start new worker with a fresh pipe
    if (process_start(worker,
                      worker_id,
                      worker_path) != 0)
    {
        pthread_mutex_lock(&g_workers_lock);

        worker->state = WORKER_FAILED;

        pthread_mutex_unlock(&g_workers_lock);

        printf("[RECOVERY] Failed to restart Worker %d.\n",
               worker_id);

        return -1;
    }

    printf("[RECOVERY] Worker %d restarted successfully. Attempt: %d\n",
           worker_id,
           restart_attempt);

    return 1;
}