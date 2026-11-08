#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "logger.h"
#include "ipc.h"

/* Starts a fake worker whose stdout goes into a pipe. */
static void spawn(WorkerProcess *w, pthread_mutex_t *lock, int i, int mode)
{
    int fds[2];
    if (pipe(fds) == -1) { perror("pipe"); return; }

    pid_t p = fork();
    if (p == 0)
    {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        setvbuf(stdout, NULL, _IOLBF, 0);

        if (mode == 0)
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
        printf("restarted worker %d is alive\n", i + 1);
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
    WorkerProcess w[2];
    memset(w, 0, sizeof w);
    w[0].pipe_read_fd = -1;
    w[1].pipe_read_fd = -1;

    log_init("tests/test_ipc.log");

    spawn(w, &lock, 0, 0);
    spawn(w, &lock, 1, 0);
    ipc_start(w, 2, &lock);
    sleep(6);

    /* simulate crash + restart of worker 2, like process_check_exit()
       and process_start() do */
    kill(w[1].pid, SIGKILL);
    waitpid(w[1].pid, NULL, 0);
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

    for (int i = 0; i < 2; i++)
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
    return 0;
}
