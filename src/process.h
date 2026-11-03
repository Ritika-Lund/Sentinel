#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>

// Possible states of a worker process
typedef enum
{
    WORKER_NOT_STARTED,
    WORKER_RUNNING,
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

} WorkerProcess;


// Process management functions

int process_start(WorkerProcess *worker,
                  int worker_id,
                  const char *worker_path);

int process_stop(WorkerProcess *worker);

int process_check_exit(WorkerProcess *worker);

const char *process_state_name(WorkerState state);

#endif