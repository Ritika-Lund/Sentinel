#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "process.h"
#include "recovery.h"

#define MAX_WORKERS 5
#define MAX_RESTARTS 3
#define MONITOR_CYCLES 12

int main(void)
{
    printf("\n");
    printf("====================================\n");
    printf("       SENTINEL PROCESS SUPERVISOR\n");
    printf("====================================\n\n");

    // Array to store all worker processes

    WorkerProcess workers[MAX_WORKERS] = {0};

    const char *worker_path = "./bin/worker";

    // Start all workers

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        if (process_start(&workers[i],
                          i + 1,
                          worker_path) != 0)
        {
            printf("[ERROR] Could not start Worker %d\n", i + 1);

            // Stop workers already started

            for (int j = 0; j < i; j++)
            {
                process_stop(&workers[j]);
            }

            return 1;
        }
    }

    printf("\n[SENTINEL] All workers started successfully.\n");

    // Monitoring loop

    for (int cycle = 1; cycle <= MONITOR_CYCLES; cycle++)
    {
        sleep(1);

        printf("\n[SENTINEL] Monitoring cycle: %d\n", cycle);

        // Simulate repeated crashes of Worker 1

        if ((cycle == 4 || cycle == 6 ||
             cycle == 8 || cycle == 10) &&
             workers[0].state == WORKER_RUNNING)
        {
            printf("\n[TEST] Simulating Worker 1 crash...\n");

            if (kill(workers[0].pid, SIGKILL) == -1)
            {
                perror("Failed to simulate crash");
            }
        }

        // Check and recover every worker

        for (int i = 0; i < MAX_WORKERS; i++)
        {
            if (workers[i].state == WORKER_RUNNING)
            {
                recovery_check_and_restart(
                    &workers[i],
                    worker_path,
                    MAX_RESTARTS
                );
            }
        }

        // Display worker information

        printf("\n========== WORKER STATUS ==========\n");

        for (int i = 0; i < MAX_WORKERS; i++)
        {
            printf("Worker %d | PID: %d | State: %s | Restarts: %d\n",
                   workers[i].worker_id,
                   workers[i].pid,
                   process_state_name(workers[i].state),
                   workers[i].restart_count);
        }

        printf("===================================\n");
    }

    // Shutdown all workers

    printf("\n[SENTINEL] Shutting down all workers...\n");

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        process_stop(&workers[i]);
    }

    printf("\n[SENTINEL] Shutdown complete.\n");

    return 0;
}