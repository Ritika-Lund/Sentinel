#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "logger.h"
#include "monitor_thread.h"

static pid_t spawn(int spin)
{
    pid_t p = fork();
    if (p == 0)
    {
        if (spin) { for (;;) { } }
        for (;;) sleep(1);
    }
    return p;
}

int main(void)
{
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    WorkerProcess w[3];
    memset(w, 0, sizeof w);

    log_init("tests/test_thread.log");

    for (int i = 0; i < 3; i++)
    {
        w[i].worker_id = i;
        w[i].pid = spawn(i != 1);      /* worker 1 sleeps, 0 and 2 spin */
        w[i].state = WORKER_RUNNING;
    }

    monitor_start(w, 3, &lock);
    sleep(6);

    /* simulate a crash of worker 2 like Deepak's recovery would */
    kill(w[2].pid, SIGKILL);
    waitpid(w[2].pid, NULL, 0);
    pthread_mutex_lock(&lock);
    w[2].state = WORKER_CRASHED;
    pthread_mutex_unlock(&lock);
    sleep(2);

    for (int i = 0; i < 2; i++)
    {
        proc_stats_t s;
        if (monitor_get_stats(i, &s) == 0)
            printf("worker %d pid=%d state=%c cpu=%.1f%%\n",
                   i, (int)s.pid, s.state, s.cpu_percent);
        else
            printf("worker %d: no stats\n", i);
    }

    monitor_stop();
    kill(w[0].pid, SIGKILL); waitpid(w[0].pid, NULL, 0);
    kill(w[1].pid, SIGKILL); waitpid(w[1].pid, NULL, 0);
    log_close();
    return 0;
}
