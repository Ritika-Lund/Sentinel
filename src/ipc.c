#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <pthread.h>
#include "ipc.h"
#include "logger.h"

#define IPC_MAX      64
#define IPC_LINE_LEN 256

static pthread_t g_thread;
static int g_started = 0;
static volatile sig_atomic_t g_stop = 0;

static WorkerProcess *s_workers = NULL;
static int g_count = 0;
static pthread_mutex_t *g_lock = NULL;

/* per-worker state, used only by the reader thread */
static char  g_buf[IPC_MAX][IPC_LINE_LEN];
static int   g_len[IPC_MAX];
static int   g_fd[IPC_MAX];
static pid_t g_pid[IPC_MAX];

static void emit_line(int i, int id)
{
    g_buf[i][g_len[i]] = '\0';
    if (g_len[i] > 0 && g_buf[i][g_len[i] - 1] == '\r')
        g_buf[i][--g_len[i]] = '\0';
    if (g_len[i] > 0)
        log_event("OUT", "Worker %d: %s", id, g_buf[i]);
    g_len[i] = 0;
}

static void feed(int i, int id, const char *data, int n)
{
    for (int k = 0; k < n; k++)
    {
        if (data[k] == '\n')
            emit_line(i, id);
        else
        {
            if (g_len[i] >= IPC_LINE_LEN - 1)   /* very long line: split it */
                emit_line(i, id);
            g_buf[i][g_len[i]++] = data[k];
        }
    }
}

static void *ipc_loop(void *arg)
{
    (void)arg;

    while (!g_stop)
    {
        for (int i = 0; i < g_count; i++)
        {
            for (int round = 0; round < 8; round++)
            {
                char chunk[256];
                int got = 0;

                /* check and read while holding the lock, so the fd cannot
                   be closed under us; poll(0) means read() cannot block */
                pthread_mutex_lock(g_lock);
                int fd = s_workers[i].pipe_read_fd;
                int id = s_workers[i].worker_id;
                pid_t pid = s_workers[i].pid;
                if (fd >= 0)
                {
                    struct pollfd p = { fd, POLLIN, 0 };
                    if (poll(&p, 1, 0) > 0 && (p.revents & POLLIN))
                    {
                        ssize_t r = read(fd, chunk, sizeof chunk);
                        if (r > 0)
                            got = (int)r;
                    }
                }
                pthread_mutex_unlock(g_lock);

                /* new pipe or new process (restart): drop half-finished line */
                if (fd != g_fd[i] || pid != g_pid[i])
                {
                    g_len[i] = 0;
                    g_fd[i] = fd;
                    g_pid[i] = pid;
                }

                if (got <= 0)
                    break;
                feed(i, id, chunk, got);
            }
        }

        for (int k = 0; k < 2 && !g_stop; k++)
            usleep(100000);
    }
    return NULL;
}

int ipc_start(WorkerProcess *workers, int count, pthread_mutex_t *lock)
{
    if (g_started || !workers || !lock || count < 1 || count > IPC_MAX)
        return -1;

    s_workers = workers;
    g_count = count;
    g_lock = lock;
    g_stop = 0;

    memset(g_len, 0, sizeof g_len);
    memset(g_pid, 0, sizeof g_pid);
    for (int i = 0; i < IPC_MAX; i++)
        g_fd[i] = -1;

    if (pthread_create(&g_thread, NULL, ipc_loop, NULL) != 0)
        return -1;
    g_started = 1;
    log_event("INFO", "IPC reader started (%d workers)", count);
    return 0;
}

void ipc_stop(void)
{
    if (!g_started)
        return;
    g_stop = 1;
    pthread_join(g_thread, NULL);
    g_started = 0;
    log_event("INFO", "IPC reader stopped");
}
