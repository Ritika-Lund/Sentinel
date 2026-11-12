#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include "ipc.h"
#include "logger.h"

#define IPC_MAX      64
#define IPC_LINE_LEN 256

static pthread_t g_thread;
static atomic_bool g_started = false;
static atomic_bool g_stop = false;

static WorkerProcess *s_workers = NULL;
static int g_count = 0;
static pthread_mutex_t *g_lock = NULL;

/* per-worker state, used only by the reader thread
   (and by ipc_stop after that thread has been joined) */
static char  g_buf[IPC_MAX][IPC_LINE_LEN];
static int   g_len[IPC_MAX];
static int   g_fd[IPC_MAX];
static pid_t g_pid[IPC_MAX];
static int   g_id[IPC_MAX];
static int   g_eof[IPC_MAX];   /* pipe ended or failed: stop polling it */

static void emit_line(int i)
{
    g_buf[i][g_len[i]] = '\0';
    if (g_len[i] > 0 && g_buf[i][g_len[i] - 1] == '\r')
        g_buf[i][--g_len[i]] = '\0';
    if (g_len[i] > 0)
        log_event("OUT", "Worker %d: %s", g_id[i], g_buf[i]);
    g_len[i] = 0;
}

static void feed(int i, const char *data, int n)
{
    for (int k = 0; k < n; k++)
    {
        if (data[k] == '\n')
            emit_line(i);
        else
        {
            if (g_len[i] >= IPC_LINE_LEN - 1)   /* very long line: split it */
                emit_line(i);
            g_buf[i][g_len[i]++] = data[k];
        }
    }
}

/* One check of worker i. Returns the number of bytes read (> 0), or 0 if
   nothing was read (no data, EOF, error or pipe not in use). */
static int read_worker(int i)
{
    char chunk[256];
    int fd, id;
    pid_t pid;

    /* 1. snapshot the worker's pipe */
    pthread_mutex_lock(g_lock);
    fd = s_workers[i].pipe_read_fd;
    id = s_workers[i].worker_id;
    pid = s_workers[i].pid;
    pthread_mutex_unlock(g_lock);

    /* 2. new pipe or new process (restart): keep what the old one wrote */
    if (fd != g_fd[i] || pid != g_pid[i])
    {
        emit_line(i);                 /* flush half-finished line, if any */
        g_fd[i] = fd;
        g_pid[i] = pid;
        g_eof[i] = 0;
    }
    g_id[i] = id;

    if (fd < 0 || g_eof[i])
        return 0;

    /* 3. check and read while holding the lock, so the fd cannot be closed
          under us; poll(0) means read() cannot block */
    int got = 0;
    int ended = 0;
    const char *problem = NULL;

    pthread_mutex_lock(g_lock);
    if (s_workers[i].pipe_read_fd == fd)
    {
        struct pollfd p = { fd, POLLIN, 0 };
        int pr = poll(&p, 1, 0);

        if (pr < 0)
        {
            if (errno != EINTR)
            {
                problem = "poll failed";
                ended = 1;
            }
        }
        else if (pr > 0)
        {
            if (p.revents & POLLNVAL)
            {
                problem = "invalid pipe descriptor (POLLNVAL)";
                ended = 1;
            }
            else if (p.revents & (POLLIN | POLLHUP | POLLERR))
            {
                /* POLLHUP/POLLERR: read() tells us the truth. It still
                   returns data that is left in the pipe, then 0 (EOF). */
                ssize_t r = read(fd, chunk, sizeof chunk);
                if (r > 0)
                    got = (int)r;
                else if (r == 0)
                    ended = 1;                              /* EOF */
                else if (errno != EINTR && errno != EAGAIN)
                {
                    problem = "read failed";
                    ended = 1;
                }
                else if (p.revents & POLLERR)
                {
                    problem = "pipe error (POLLERR)";
                    ended = 1;
                }
            }
        }
    }
    pthread_mutex_unlock(g_lock);

    if (got > 0)
        feed(i, chunk, got);

    if (ended)
    {
        emit_line(i);                 /* flush the last line without '\n' */
        g_eof[i] = 1;
        if (problem)
            log_event("WARN", "Worker %d: %s, stopped reading its output",
                      id, problem);
        else
            log_event("INFO", "Worker %d: output pipe closed (EOF)", id);
    }
    return got;
}

static void *ipc_loop(void *arg)
{
    (void)arg;

    while (!atomic_load(&g_stop))
    {
        for (int i = 0; i < g_count; i++)
            for (int round = 0; round < 8 && read_worker(i) > 0; round++)
                ;

        for (int k = 0; k < 2 && !atomic_load(&g_stop); k++)
            usleep(100000);
    }
    return NULL;
}

int ipc_start(WorkerProcess *workers, int count, pthread_mutex_t *lock)
{
    if (atomic_load(&g_started) || !workers || !lock ||
        count < 1 || count > IPC_MAX)
        return -1;

    s_workers = workers;
    g_count = count;
    g_lock = lock;
    atomic_store(&g_stop, false);

    memset(g_len, 0, sizeof g_len);
    memset(g_pid, 0, sizeof g_pid);
    memset(g_id, 0, sizeof g_id);
    memset(g_eof, 0, sizeof g_eof);
    for (int i = 0; i < IPC_MAX; i++)
        g_fd[i] = -1;

    if (pthread_create(&g_thread, NULL, ipc_loop, NULL) != 0)
        return -1;
    atomic_store(&g_started, true);
    log_event("INFO", "IPC reader started (%d workers)", count);
    return 0;
}

void ipc_stop(void)
{
    if (!atomic_exchange(&g_started, false))
        return;
    atomic_store(&g_stop, true);
    pthread_join(g_thread, NULL);

    /* final drain: read everything still waiting in the pipes, then flush
       half-finished lines, so no output is lost at shutdown */
    for (int i = 0; i < g_count; i++)
    {
        while (read_worker(i) > 0)
            ;
        emit_line(i);
    }
    log_event("INFO", "IPC reader stopped");
}
