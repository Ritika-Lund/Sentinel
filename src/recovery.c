#include <stdio.h>

#include "recovery.h"


int recovery_check_and_restart(WorkerProcess *worker,
                               const char *worker_path,
                               int max_restarts)
{
    int result = process_check_exit(worker);

    if (result == 0)
    {
        // Worker is still running
        return 0;
    }

    if (result == -1)
    {
        return -1;
    }

    // Do not restart a worker that stopped normally

    if (worker->state != WORKER_CRASHED)
    {
        return 0;
    }

    // Check maximum restart limit

    if (worker->restart_count >= max_restarts)
    {
        worker->state = WORKER_FAILED;

        printf("[RECOVERY] Worker %d reached maximum restart limit.\n",
               worker->worker_id);

        printf("[STATE] Worker %d: %s\n",
               worker->worker_id,
               process_state_name(worker->state));

        return 2;
    }

    printf("[RECOVERY] Restarting Worker %d...\n",
           worker->worker_id);

    worker->state = WORKER_RESTARTING;

    printf("[STATE] Worker %d: %s\n",
           worker->worker_id,
           process_state_name(worker->state));

    worker->restart_count++;

    if (process_start(worker,
                      worker->worker_id,
                      worker_path) != 0)
    {
        worker->state = WORKER_FAILED;

        printf("[RECOVERY] Failed to restart Worker %d.\n",
               worker->worker_id);

        return -1;
    }

    printf("[RECOVERY] Worker %d restarted successfully. Attempt: %d\n",
           worker->worker_id,
           worker->restart_count);

    return 1;
}