#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include "ipc.h"
#include "process.h"


// Global worker list
WorkerProcess g_workers[MAX_WORKERS];
int g_worker_count = 0;

// Global mutex
pthread_mutex_t g_workers_lock = PTHREAD_MUTEX_INITIALIZER;


// Close a worker's pipe descriptor
static void close_worker_pipe_locked(WorkerProcess *worker)
{
    if (worker->pipe_read_fd >= 0)
    {
        close(worker->pipe_read_fd);
        worker->pipe_read_fd = -1;
    }
}


// Return a readable name for each worker state
const char *process_state_name(WorkerState state)
{
    switch (state)
    {
        case WORKER_NOT_STARTED:
            return "NOT STARTED";

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

    // Create pipe before fork
    int fds[2];

    if (pipe2(fds, O_CLOEXEC) == -1)
    {
        perror("pipe2 failed");

        pthread_mutex_lock(&g_workers_lock);
        worker->state = WORKER_FAILED;
        pthread_mutex_unlock(&g_workers_lock);

        return -1;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");

        close(fds[0]);
        close(fds[1]);

        pthread_mutex_lock(&g_workers_lock);
        worker->state = WORKER_FAILED;
        pthread_mutex_unlock(&g_workers_lock);

        return -1;
    }

    if (pid == 0)
    {
        // Child process

        // Redirect worker stdout to pipe
        if (dup2(fds[1], STDOUT_FILENO) == -1)
        {
            perror("dup2 failed");
            _exit(127);
        }

        close(fds[0]);
        close(fds[1]);

        execl(worker_path,
              worker_path,
              id_string,
              (char *)NULL);

        perror("exec failed");
        _exit(127);
    }

    // Parent process

    close(fds[1]);

    pthread_mutex_lock(&g_workers_lock);

    worker->worker_id = worker_id;
    worker->pid = pid;
    worker->state = WORKER_RUNNING;
    worker->pipe_read_fd = fds[0];

    pthread_mutex_unlock(&g_workers_lock);

    printf("[SENTINEL] Worker %d created. PID: %d\n",
           worker_id, pid);

    printf("[STATE] Worker %d: %s\n",
           worker_id,
           process_state_name(WORKER_RUNNING));

    return 0;
}


// Check whether a worker has terminated
int process_check_exit(WorkerProcess *worker)
{
    if (worker == NULL)
    {
        return -1;
    }

    // Read PID safely
    pthread_mutex_lock(&g_workers_lock);

    pid_t worker_pid = worker->pid;

    pthread_mutex_unlock(&g_workers_lock);

    if (worker_pid <= 0)
    {
        return -1;
    }

    int status;
    pid_t result;

    // Do not hold mutex during waitpid
    do
    {
        result = waitpid(worker_pid, &status, WNOHANG);

    } while (result == -1 && errno == EINTR);

    if (result == 0)
    {
        return 0;
    }

    if (result == -1)
    {
        perror("waitpid failed");
        return -1;
    }

    /* Drain output only after waitpid confirms the worker has exited. */
    if (ipc_drain_worker(worker) != 0)
    {
        pthread_mutex_lock(&g_workers_lock);

        close_worker_pipe_locked(worker);

        pthread_mutex_unlock(&g_workers_lock);
    }

    pthread_mutex_lock(&g_workers_lock);

    // Ensure this is still the same worker process
    if (worker->pid != worker_pid)
    {
        pthread_mutex_unlock(&g_workers_lock);
        return -1;
    }

    worker->pid = 0;

    // Close old pipe
    close_worker_pipe_locked(worker);

    if (WIFEXITED(status))
    {
        int exit_code = WEXITSTATUS(status);

        if (exit_code == 0)
        {
            worker->state = WORKER_STOPPED;
        }
        else
        {
            worker->state = WORKER_CRASHED;
        }
    }
    else if (WIFSIGNALED(status))
    {
        worker->state = WORKER_CRASHED;
    }

    WorkerState new_state = worker->state;
    int id = worker->worker_id;

    pthread_mutex_unlock(&g_workers_lock);

    if (WIFEXITED(status))
    {
        int exit_code = WEXITSTATUS(status);

        if (exit_code == 0)
        {
            printf("[SENTINEL] Worker %d exited normally.\n", id);
        }
        else
        {
            printf("[WARNING] Worker %d exited unexpectedly. Code: %d\n",
                   id, exit_code);
        }
    }
    else if (WIFSIGNALED(status))
    {
        printf("[WARNING] Worker %d crashed due to signal %d\n",
               id, WTERMSIG(status));
    }

    printf("[STATE] Worker %d: %s\n",
           id, process_state_name(new_state));

    return 1;
}


// Pause a running worker
int process_pause(WorkerProcess *worker)
{
    if (worker == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&g_workers_lock);

    pid_t pid = worker->pid;
    WorkerState state = worker->state;

    pthread_mutex_unlock(&g_workers_lock);

    if (pid <= 0 || state != WORKER_RUNNING)
    {
        return -1;
    }

    if (kill(pid, SIGSTOP) == -1)
    {
        perror("Failed to pause worker");
        return -1;
    }

    pthread_mutex_lock(&g_workers_lock);

    if (worker->pid == pid)
    {
        worker->state = WORKER_PAUSED;
    }

    pthread_mutex_unlock(&g_workers_lock);

    printf("[SENTINEL] Worker %d paused.\n",
           worker->worker_id);

    return 0;
}


// Resume a paused worker
int process_resume(WorkerProcess *worker)
{
    if (worker == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&g_workers_lock);

    pid_t pid = worker->pid;
    WorkerState state = worker->state;

    pthread_mutex_unlock(&g_workers_lock);

    if (pid <= 0 || state != WORKER_PAUSED)
    {
        return -1;
    }

    if (kill(pid, SIGCONT) == -1)
    {
        perror("Failed to resume worker");
        return -1;
    }

    pthread_mutex_lock(&g_workers_lock);

    if (worker->pid == pid)
    {
        worker->state = WORKER_RUNNING;
    }

    pthread_mutex_unlock(&g_workers_lock);

    printf("[SENTINEL] Worker %d resumed.\n",
           worker->worker_id);

    return 0;
}


// Stop a running or paused worker
int process_stop(WorkerProcess *worker)
{
    if (worker == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&g_workers_lock);

    pid_t pid = worker->pid;
    WorkerState state = worker->state;

    pthread_mutex_unlock(&g_workers_lock);

    if (pid <= 0)
    {
        return 0;
    }

    printf("[SENTINEL] Stopping worker %d (PID: %d)\n",
           worker->worker_id, pid);

    // Resume paused worker before termination
    if (state == WORKER_PAUSED)
    {
        if (kill(pid, SIGCONT) == -1 && errno != ESRCH)
        {
            perror("Failed to resume paused worker");
            return -1;
        }
    }

    if (kill(pid, SIGTERM) == -1 && errno != ESRCH)
    {
        perror("kill failed");
        return -1;
    }

    int status;
    pid_t result;

    // Do not hold mutex during blocking waitpid
    do
    {
        result = waitpid(pid, &status, 0);

    } while (result == -1 && errno == EINTR);

    if (result == -1)
    {
        perror("waitpid failed");
        return -1;
    }

    /*
 * Drain remaining worker output before closing its pipe.
 */
if (ipc_drain_worker(worker) != 0)
{
    pthread_mutex_lock(&g_workers_lock);

    close_worker_pipe_locked(worker);

    pthread_mutex_unlock(&g_workers_lock);
}
    pthread_mutex_lock(&g_workers_lock);

    if (worker->pid == pid)
    {
        worker->pid = 0;
        worker->state = WORKER_STOPPED;

        close_worker_pipe_locked(worker);
    }

    pthread_mutex_unlock(&g_workers_lock);

    printf("[STATE] Worker %d: STOPPED\n",
           worker->worker_id);

    return 0;
}