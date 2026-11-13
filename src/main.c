#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include "ipc.h"

#include "process.h"
#include "recovery.h"
#include "monitor_thread.h"
#include "logger.h"
#include "scheduler.h"
#define DEFAULT_WORKERS 5
#define MAX_RESTARTS 3
#define MONITOR_CYCLES 12

volatile sig_atomic_t shutdown_requested = 0;


/* Handle Ctrl+C */
void handle_sigint(int signal)
{
    (void)signal;
    shutdown_requested = 1;
}

static void report_schedule(const char *name, const Task *tasks, int count)
{
    double total_waiting = 0;
    double total_turnaround = 0;

    print_results(name, tasks, count);
    log_event("INFO", "Scheduler %s results (%d simulated worker tasks)",
              name, count);

    for (int i = 0; i < count; i++)
    {
        log_event("INFO",
                  "Scheduler %s Worker %d (PID %d): arrival=%d burst=%d start=%d finish=%d waiting=%d turnaround=%d",
                  name, tasks[i].id, (int)tasks[i].pid,
                  tasks[i].arrival, tasks[i].burst, tasks[i].start,
                  tasks[i].finish, tasks[i].waiting, tasks[i].turnaround);
        total_waiting += tasks[i].waiting;
        total_turnaround += tasks[i].turnaround;
    }

    log_event("INFO", "Scheduler %s averages: waiting=%.2f turnaround=%.2f",
              name, total_waiting / count, total_turnaround / count);
}

static void run_scheduler_simulation(int worker_count)
{
    Task base[MAX_WORKERS];
    Task fcfs[MAX_WORKERS];
    Task sjf[MAX_WORKERS];
    Task round_robin[MAX_WORKERS];

    pthread_mutex_lock(&g_workers_lock);
    for (int i = 0; i < worker_count; i++)
    {
        task_init(&base[i], g_workers[i].worker_id, g_workers[i].pid,
                  0, worker_count - i);
    }
    pthread_mutex_unlock(&g_workers_lock);

    for (int i = 0; i < worker_count; i++)
    {
        fcfs[i] = base[i];
        sjf[i] = base[i];
        round_robin[i] = base[i];
    }

    printf("\n[SENTINEL] Scheduler simulation (synthetic burst units; no effect on Linux scheduling).\n");
    log_event("INFO",
              "Starting scheduler simulation: one task per worker, arrival=0, synthetic burst units descend by worker number");

    schedule_fcfs(fcfs, worker_count);
    report_schedule("FCFS", fcfs, worker_count);

    schedule_sjf(sjf, worker_count);
    report_schedule("SJF", sjf, worker_count);

    schedule_rr(round_robin, worker_count, 2);
    report_schedule("Round Robin (quantum=2)", round_robin, worker_count);
}


int main(int argc, char *argv[])
{
    int worker_count = DEFAULT_WORKERS;

    /* Validate command-line arguments */

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

    /* Initialize logger */

    if (log_init("sentinel.log") != 0)
    {
        fprintf(stderr, "[ERROR] Failed to initialize logger.\n");
        return 1;
    }

    log_event("INFO", "SENTINEL started");

    /* Register Ctrl+C handler */

    if (signal(SIGINT, handle_sigint) == SIG_ERR)
    {
        perror("Failed to register SIGINT handler");
        log_event("ERROR", "Failed to register SIGINT handler");
        log_close();
        return 1;
    }

    printf("\n");
    printf("====================================\n");
    printf("       SENTINEL PROCESS SUPERVISOR\n");
    printf("====================================\n\n");

    printf("[SENTINEL] Configured workers: %d\n\n",
           worker_count);

    log_event("INFO", "Configured workers: %d", worker_count);

    /* Initialize global worker list */

    pthread_mutex_lock(&g_workers_lock);

    g_worker_count = worker_count;

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        g_workers[i].worker_id = 0;
        g_workers[i].pid = 0;
        g_workers[i].restart_count = 0;
        g_workers[i].state = WORKER_NOT_STARTED;
        g_workers[i].pipe_read_fd = -1;
    }

    pthread_mutex_unlock(&g_workers_lock);

    const char *worker_path = "./bin/worker";

    /* Start workers */

    for (int i = 0; i < worker_count; i++)
    {
        if (process_start(&g_workers[i],
                          i + 1,
                          worker_path) != 0)
        {
            printf("[ERROR] Could not start Worker %d\n", i + 1);

            log_event("ERROR", "Could not start Worker %d", i + 1);

            for (int j = 0; j < i; j++)
            {
                process_stop(&g_workers[j]);
            }

            pthread_mutex_lock(&g_workers_lock);
            g_worker_count = 0;
            pthread_mutex_unlock(&g_workers_lock);

            log_event("INFO", "SENTINEL startup aborted");
            log_close();

            return 1;
        }
    }

    printf("\n[SENTINEL] All workers started successfully.\n");
    printf("[SENTINEL] Press Ctrl+C to stop all workers.\n");

    log_event("INFO", "All workers started successfully");

    /* Start background monitoring thread */

    if (monitor_start(g_workers,
                      worker_count,
                      &g_workers_lock) != 0)
    {
        printf("[ERROR] Could not start monitoring thread.\n");

        log_event("ERROR", "Could not start monitoring thread");

        for (int i = 0; i < worker_count; i++)
        {
            process_stop(&g_workers[i]);
        }

        log_event("INFO", "SENTINEL shutdown after monitor startup failure");
        log_close();

        return 1;
    }

    if (ipc_start(g_workers, worker_count, &g_workers_lock) != 0)
    {
        printf("[ERROR] Could not start IPC reader.\n");
        log_event("ERROR", "Could not start IPC reader");

        monitor_stop();

        for (int i = 0; i < worker_count; i++)
            process_stop(&g_workers[i]);

        log_close();
        return 1;
    }

    run_scheduler_simulation(worker_count);

    /* Main monitoring and recovery loop */

    for (int cycle = 1;
         cycle <= MONITOR_CYCLES && !shutdown_requested;
         cycle++)
    {
        sleep(1);

        if (shutdown_requested)
            break;

        printf("\n[SENTINEL] Monitoring cycle: %d\n", cycle);

        log_event("INFO", "Monitoring cycle: %d", cycle);

        /* Crash simulation for testing */

        pthread_mutex_lock(&g_workers_lock);

        pid_t first_worker_pid = g_workers[0].pid;
        WorkerState first_worker_state = g_workers[0].state;

        pthread_mutex_unlock(&g_workers_lock);

        if ((cycle == 4 || cycle == 6 ||
             cycle == 8 || cycle == 10) &&
             first_worker_state == WORKER_RUNNING &&
             first_worker_pid > 0)
        {
            printf("\n[TEST] Simulating Worker 1 crash...\n");

            log_event("WARNING", "Simulating Worker 1 crash");

            if (kill(first_worker_pid, SIGKILL) == -1)
            {
                perror("Failed to simulate crash");

                log_event("ERROR",
                          "Failed to simulate Worker 1 crash");
            }
        }

        /* Check all workers and recover crashed workers */

        for (int i = 0; i < worker_count; i++)
        {
            pthread_mutex_lock(&g_workers_lock);

            WorkerState state = g_workers[i].state;

            pthread_mutex_unlock(&g_workers_lock);

            if (state == WORKER_RUNNING ||
                state == WORKER_PAUSED)
            {
                recovery_check_and_restart(
                    &g_workers[i],
                    worker_path,
                    MAX_RESTARTS
                );
            }
        }

        /* Display worker status */

        printf("\n========== WORKER STATUS ==========\n");

        for (int i = 0; i < worker_count; i++)
        {
            pthread_mutex_lock(&g_workers_lock);
            WorkerProcess worker = g_workers[i];
            pthread_mutex_unlock(&g_workers_lock);

            proc_stats_t stats;
            if (monitor_get_stats(i, &stats) == 0 &&
                stats.pid == worker.pid)
            {
                printf("Worker %d | PID: %d | State: %s | CPU: %.1f%% | Restarts: %d\n",
                       worker.worker_id, worker.pid,
                       process_state_name(worker.state), stats.cpu_percent,
                       worker.restart_count);
                log_event("INFO",
                          "Worker %d status: PID=%d state=%s CPU=%.1f%% restarts=%d",
                          worker.worker_id, (int)worker.pid,
                          process_state_name(worker.state), stats.cpu_percent,
                          worker.restart_count);
            }
            else
            {
                printf("Worker %d | PID: %d | State: %s | CPU: n/a | Restarts: %d\n",
                       worker.worker_id, worker.pid,
                       process_state_name(worker.state),
                       worker.restart_count);
                log_event("INFO",
                          "Worker %d status: PID=%d state=%s CPU=n/a restarts=%d",
                          worker.worker_id, (int)worker.pid,
                          process_state_name(worker.state),
                          worker.restart_count);
            }
        }

        printf("===================================\n");
    }

    /* Graceful shutdown */

    printf("\n[SENTINEL] Shutting down all workers...\n");
    monitor_stop();

    for (int i = 0; i < worker_count; i++)
    {
        process_stop(&g_workers[i]);
    }

    ipc_stop();

    log_event("INFO", "SENTINEL shutdown complete");

    printf("\n[SENTINEL] Shutdown complete.\n");

    log_close();

    return 0;
}