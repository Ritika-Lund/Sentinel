#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "logger.h"
#include "ipc.h"
#include "test_util.h"

#define LOG "tests/test_ipc.log"

/* Starts a fake worker whose stdout goes into a pipe (like process_start). */
static void spawn(WorkerProcess *w, pthread_mutex_t *lock, int i, int mode)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t p = fork();
    CHECK(p >= 0);
    if (p == 0)
    {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        setvbuf(stdout, NULL, _IOLBF, 0);

        if (mode == 0)          /* normal worker, runs until killed */
        {
            for (int k = 1; k <= 3; k++)
            {
                printf("[WORKER %d] Working... Task %d\n", i + 1, k);
                sleep(1);
            }
            printf("hel");                 /* one line, written in two parts */
            fflush(stdout);
            sleep(1);
            printf("lo from worker %d\n", i + 1);
            for (;;) sleep(1);
        }
        if (mode == 1)          /* restarted worker */
        {
            printf("restarted worker %d is alive\n", i + 1);
            for (;;) sleep(1);
        }
        if (mode == 2)          /* prints, then exits: EOF, last line has no '\n' */
        {
            printf("last words");
            fflush(stdout);
            _exit(0);
        }
        /* mode 3: writes a burst right before shutdown */
        for (int k = 1; k <= 5; k++)
            printf("burst %d\n", k);
        fflush(stdout);
        for (;;) sleep(1);
    }

    close(fds[1]);
    pthread_mutex_lock(lock);
    w[i].worker_id = i + 1;
    w[i].pid = p;
    w[i].state = WORKER_RUNNING;
    w[i].pipe_read_fd = fds[0];
    pthread_mutex_unlock(lock);
}

int main(void)
{
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    WorkerProcess w[4];
    memset(w, 0, sizeof w);
    for (int i = 0; i < 4; i++)
        w[i].pipe_read_fd = -1;
    remove(LOG);

    CHECK(log_init(LOG) == 0);

    CHECK(ipc_start(NULL, 2, &lock) == -1);
    CHECK(ipc_start(w, 2, NULL) == -1);
    CHECK(ipc_start(w, 0, &lock) == -1);

    spawn(w, &lock, 0, 0);
    spawn(w, &lock, 1, 0);
    spawn(w, &lock, 2, 2);             /* exits right away: EOF case */
    CHECK(ipc_start(w, 3, &lock) == 0);
    CHECK(ipc_start(w, 3, &lock) == -1);    /* already running */
    sleep(6);

    /* simulate crash + restart of worker 2, like process_check_exit()
       and process_start() do */
    CHECK(kill(w[1].pid, SIGKILL) == 0);
    CHECK(waitpid(w[1].pid, NULL, 0) == w[1].pid);
    pthread_mutex_lock(&lock);
    close(w[1].pipe_read_fd);
    w[1].pipe_read_fd = -1;
    w[1].pid = 0;
    w[1].state = WORKER_CRASHED;
    pthread_mutex_unlock(&lock);
    sleep(1);

    spawn(w, &lock, 1, 1);
    sleep(2);

    ipc_stop();
    ipc_stop();                         /* second stop is safe */

    for (int i = 0; i < 3; i++)
    {
        if (w[i].pid > 0)
        {
            kill(w[i].pid, SIGKILL);
            waitpid(w[i].pid, NULL, 0);
        }
        if (w[i].pipe_read_fd >= 0)
            close(w[i].pipe_read_fd);
    }
    log_close();

    CHECK(count_in_file(LOG, "IPC reader started (3 workers)") == 1);
    CHECK(count_in_file(LOG, "Worker 1: [WORKER 1] Working... Task 3") == 1);
    CHECK(count_in_file(LOG, "Worker 2: [WORKER 2] Working... Task 1") == 1);
    CHECK(count_in_file(LOG, "Worker 1: hello from worker 1") == 1);   /* joined line */
    CHECK(count_in_file(LOG, "Worker 2: hello from worker 2") == 1);
    CHECK(count_in_file(LOG, "Worker 2: restarted worker 2 is alive") == 1);
    CHECK(count_in_file(LOG, "Worker 3: last words") == 1);            /* no '\n' flushed */
    CHECK(count_in_file(LOG, "Worker 3: output pipe closed (EOF)") == 1);
    CHECK(count_in_file(LOG, "IPC reader stopped") == 1);

    /* second scenario: data waiting in the pipe at shutdown */
    {
        pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;
        WorkerProcess v[1];
        memset(v, 0, sizeof v);
        v[0].pipe_read_fd = -1;
        remove(LOG);
        CHECK(log_init(LOG) == 0);

        CHECK(ipc_start(v, 1, &lock2) == 0);
        spawn(v, &lock2, 0, 3);
        usleep(300000);                  /* worker has written; reader may not have polled yet */
        ipc_stop();                      /* must drain what is left */
        kill(v[0].pid, SIGKILL);
        waitpid(v[0].pid, NULL, 0);
        close(v[0].pipe_read_fd);
        log_close();

        for (int k = 1; k <= 5; k++)
        {
            char needle[64];
            snprintf(needle, sizeof needle, "Worker 1: burst %d", k);
            CHECK(count_in_file(LOG, needle) == 1);
        }
    }

    printf("test_ipc: ALL PASSED\n");
    return 0;
} 
