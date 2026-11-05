#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#include "process.h"


// Return a readable name for each worker state
const char *process_state_name(WorkerState state)
{
    switch (state)
    {
        case WORKER_NOT_STARTED:
            return "NOT STARTED";

        case WORKER_RUNNING:
            return "RUNNING";

        case WORKER_CRASHED:
            return "CRASHED";
        
        case WORKER_PAUSED:
            return "PAUSED";
        
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


// Create and execute a worker
int process_start(WorkerProcess *worker,
                  int worker_id,
                  const char *worker_path)
{
    if (worker == NULL || worker_path == NULL)
    {
        return -1;
    }

    char id_string[20];

    snprintf(id_string, sizeof(id_string),
             "%d", worker_id);

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        worker->state = WORKER_FAILED;
        return -1;
    }

    if (pid == 0)
    {
        // Child process

        execl(worker_path,
              worker_path,
              id_string,
              (char *)NULL);

        perror("exec failed");
        _exit(127);
    }

    // Parent process

    worker->worker_id = worker_id;
    worker->pid = pid;
    worker->state = WORKER_RUNNING;

    printf("[SENTINEL] Worker %d created. PID: %d\n",
           worker_id, pid);

    printf("[STATE] Worker %d: %s\n",
           worker_id,
           process_state_name(worker->state));

    return 0;
}


// Check whether a worker has terminated
int process_check_exit(WorkerProcess *worker)
{
    if (worker == NULL || worker->pid <= 0)
    {
        return -1;
    }

    int status;
    pid_t result;

    do
    {
        result = waitpid(worker->pid, &status, WNOHANG);

    } while (result == -1 && errno == EINTR);

    if (result == 0)
    {
        // Worker is still running
        return 0;
    }

    if (result == -1)
    {
        perror("waitpid failed");
        return -1;
    }

    // Worker has terminated

    worker->pid = 0;

    if (WIFEXITED(status))
    {
        int exit_code = WEXITSTATUS(status);

        if (exit_code == 0)
        {
            worker->state = WORKER_STOPPED;

            printf("[SENTINEL] Worker %d exited normally.\n",
                   worker->worker_id);
        }
        else
        {
            worker->state = WORKER_CRASHED;

            printf("[WARNING] Worker %d exited unexpectedly. Code: %d\n",
                   worker->worker_id, exit_code);
        }
    }
    else if (WIFSIGNALED(status))
    {
        worker->state = WORKER_CRASHED;

        printf("[WARNING] Worker %d crashed due to signal %d\n",
               worker->worker_id, WTERMSIG(status));
    }

    printf("[STATE] Worker %d: %s\n",
           worker->worker_id,
           process_state_name(worker->state));

    return 1;
}// Pause a running worker
int process_pause(WorkerProcess *worker)
{
    if (worker == NULL || worker->pid <= 0)
    {
        return -1;
    }

    if (worker->state != WORKER_RUNNING)
    {
        return -1;
    }

    if (kill(worker->pid, SIGSTOP) == -1)
    {
        perror("Failed to pause worker");
        return -1;
    }

    worker->state = WORKER_PAUSED;

    printf("[SENTINEL] Worker %d paused.\n",
           worker->worker_id);

    printf("[STATE] Worker %d: %s\n",
           worker->worker_id,
           process_state_name(worker->state));

    return 0;
}
// Resume a paused worker
int process_resume(WorkerProcess *worker)
{
    if (worker == NULL || worker->pid <= 0)
    {
        return -1;
    }

    if (worker->state != WORKER_PAUSED)
    {
        return -1;
    }

    if (kill(worker->pid, SIGCONT) == -1)
    {
        perror("Failed to resume worker");
        return -1;
    }

    worker->state = WORKER_RUNNING;

    printf("[SENTINEL] Worker %d resumed.\n",
           worker->worker_id);

    printf("[STATE] Worker %d: %s\n",
           worker->worker_id,
           process_state_name(worker->state));

    return 0;
}



// Stop a running worker

// Stop a running or paused worker
int process_stop(WorkerProcess *worker)
{
    if (worker == NULL)
    {
        return -1;
    }

    if (worker->pid <= 0)
    {
        return 0;
    }

    printf("[SENTINEL] Stopping worker %d (PID: %d)\n",
           worker->worker_id, worker->pid);

    // Resume a paused worker before sending SIGTERM
    if (worker->state == WORKER_PAUSED)
    {
        if (kill(worker->pid, SIGCONT) == -1 && errno != ESRCH)
        {
            perror("Failed to resume paused worker");
            return -1;
        }
    }

    // Request worker termination
    if (kill(worker->pid, SIGTERM) == -1 && errno != ESRCH)
    {
        perror("kill failed");
        return -1;
    }

    // Wait for worker termination
    int status;
    pid_t result;

    do
    {
        result = waitpid(worker->pid, &status, 0);

    } while (result == -1 && errno == EINTR);

    if (result == -1)
    {
        perror("waitpid failed");
        return -1;
    }

    worker->pid = 0;
    worker->state = WORKER_STOPPED;

    printf("[STATE] Worker %d: %s\n",
           worker->worker_id,
           process_state_name(worker->state));

    return 0;
}