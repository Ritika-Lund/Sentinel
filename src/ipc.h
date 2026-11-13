#ifndef IPC_H
#define IPC_H

#include <pthread.h>
#include "process.h"

int ipc_start(WorkerProcess *workers, int count, pthread_mutex_t *lock);

/* Drain a terminated worker's output before its pipe is closed. */
int ipc_drain_worker(WorkerProcess *worker);

void ipc_stop(void);

#endif