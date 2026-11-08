#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>
#include <pthread.h>

#define MAX_WORKERS 10

// Possible states of a worker process
typedef enum
{
    WORKER_NOT_STARTED,
    WORKER_RUNNING,
    WORKER_PAUSED,
    WORKER_CRASHED,
    WORKER_RESTARTING,
    WORKER_STOPPED,
    WORKER_FAILED

} WorkerState;


// Information about each worker
typedef struct
{
    int worker_id;
    pid_t pid;
    int restart_count;
    WorkerState state;
    int pipe_read_fd;

} WorkerProcess;


// Global worker list and synchronization
extern WorkerProcess g_workers[MAX_WORKERS];
extern int g_worker_count;
extern pthread_mutex_t g_workers_lock;


// Process management functions

int process_start(WorkerProcess *worker,
                  int worker_id,
                  const char *worker_path);

int process_stop(WorkerProcess *worker);

int process_pause(WorkerProcess *worker);

int process_resume(WorkerProcess *worker);

int process_check_exit(WorkerProcess *worker);

const char *process_state_name(WorkerState state);

#endif