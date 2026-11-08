#ifndef IPC_H
#define IPC_H

#include <pthread.h>
#include "process.h"

/* Starts a thread that reads each worker's output pipe and logs every
   line. workers/count: the worker array. lock: the mutex that protects it.
   Returns 0 on success, -1 on error. */
int  ipc_start(WorkerProcess *workers, int count, pthread_mutex_t *lock);

/* Stops the thread and waits for it. Safe to call twice. */
void ipc_stop(void);

#endif
