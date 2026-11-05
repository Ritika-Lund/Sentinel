#ifndef RECOVERY_H
#define RECOVERY_H

#include "process.h"

int recovery_check_and_restart(WorkerProcess *worker,
                               const char *worker_path,
                               int max_restarts);

#endif