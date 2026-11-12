#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "logger.h"
#include "monitor_thread.h"
#include "test_util.h"

#define LOG "tests/test_thread.log"

static pid_t spawn(int spin)
{
    pid_t p = fork();
    CHECK(p >= 0);
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
    proc_stats_t s;
    memset(w, 0, sizeof w);
    remove(LOG);

    CHECK(log_init(LOG) == 0);

    /* argument validation, nothing started */
    CHECK(monitor_start(NULL, 3, &lock) == -1);
    CHECK(monitor_start(w, 3, NULL) == -1);
    CHECK(monitor_start(w, 0, &lock) == -1);
    CHECK(monitor_get_stats(-1, &s) == -1);
    CHECK(monitor_get_stats(0, NULL) == -1);
    CHECK(monitor_get_stats(0, &s) == -1);      /* no data yet */

    for (int i = 0; i < 3; i++)
    {
        w[i].worker_id = i;
        w[i].pid = spawn(i != 1);      /* worker 1 sleeps, 0 and 2 spin */
        w[i].state = WORKER_RUNNING;
    }

    CHECK(monitor_start(w, 3, &lock) == 0);
    CHECK(monitor_start(w, 3, &lock) == -1);    /* already running */
    sleep(6);

    /* simulate a crash of worker 2 like Deepak's recovery would */
    CHECK(kill(w[2].pid, SIGKILL) == 0);
    CHECK(waitpid(w[2].pid, NULL, 0) == w[2].pid);
    pthread_mutex_lock(&lock);
    w[2].state = WORKER_CRASHED;
    pthread_mutex_unlock(&lock);
    sleep(2);

    CHECK(monitor_get_stats(0, &s) == 0);
    printf("worker 0 pid=%d state=%c cpu=%.1f%%\n", (int)s.pid, s.state, s.cpu_percent);
    CHECK(s.pid == w[0].pid);
    CHECK(s.cpu_percent > 30.0);

    CHECK(monitor_get_stats(1, &s) == 0);
    printf("worker 1 pid=%d state=%c cpu=%.1f%%\n", (int)s.pid, s.state, s.cpu_percent);
    CHECK(s.pid == w[1].pid);
    CHECK(s.state == 'S');
    CHECK(s.cpu_percent < 10.0);

    monitor_stop();
    monitor_stop();                             /* second stop is safe */

    CHECK(kill(w[0].pid, SIGKILL) == 0); CHECK(waitpid(w[0].pid, NULL, 0) == w[0].pid);
    CHECK(kill(w[1].pid, SIGKILL) == 0); CHECK(waitpid(w[1].pid, NULL, 0) == w[1].pid);
    log_close();

    CHECK(count_in_file(LOG, "Monitor thread started (3 workers)") == 1);
    CHECK(count_in_file(LOG, "NOT_STARTED -> RUNNING") == 3);
    CHECK(count_in_file(LOG, "Worker 2 state: RUNNING -> CRASHED") == 1);
    CHECK(count_in_file(LOG, "Monitor thread stopped") == 1);

    printf("test_monitor_thread: ALL PASSED\n");
    return 0;
} 
