#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "process.h"
#include "recovery.h"

#define MAX_WORKERS 10
#define DEFAULT_WORKERS 5
#define MAX_RESTARTS 3
#define MONITOR_CYCLES 12


int main(int argc, char *argv[])
{
    int worker_count = DEFAULT_WORKERS;

    // Read worker count from command-line argument

    if (argc > 2)
    {
        printf("Usage: %s [number_of_workers]\n", argv[0]);
        return 1;
    }

    if (argc == 2)
    {
        char *end;
        errno = 0;

        long value = strtol(argv[1], &end, 10);

        if (errno != 0 || *argv[1] == '\0' ||
            *end != '\0' || value < 1 ||
            value > MAX_WORKERS)
        {
            printf("Invalid worker count.\n");
            printf("Enter a number between 1 and %d.\n",
                   MAX_WORKERS);
            return 1;
        }

        worker_count = (int)value;
    }

    printf("\n");
    printf("====================================\n");
    printf("       SENTINEL PROCESS SUPERVISOR\n");
    printf("====================================\n\n");

    printf("[SENTINEL] Configured workers: %d\n\n",
           worker_count);

    WorkerProcess workers[MAX_WORKERS] = {0};

    const char *worker_path = "./bin/worker";

    // Create workers

    for (int i = 0; i < worker_count; i++)
    {
        if (process_start(&workers[i],
                          i + 1,
                          worker_path) != 0)
        {
            printf("[ERROR] Could not start Worker %d\n",
                   i + 1);

            for (int j = 0; j < i; j++)
            {
                process_stop(&workers[j]);
            }

            return 1;
        }
    }

    printf("\n[SENTINEL] All workers started successfully.\n");

    // Monitoring loop

    for (int cycle = 1;
         cycle <= MONITOR_CYCLES;
         cycle++)
    {
        sleep(1);

        printf("\n[SENTINEL] Monitoring cycle: %d\n",
               cycle);

        // Crash simulation for Worker 1 only

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

        // Check all workers

        for (int i = 0; i < worker_count; i++)
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

        // Display worker status

        printf("\n========== WORKER STATUS ==========\n");

        for (int i = 0; i < worker_count; i++)
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

    for (int i = 0; i < worker_count; i++)
    {
        process_stop(&workers[i]);
    }

    printf("\n[SENTINEL] Shutdown complete.\n");

    return 0;
}