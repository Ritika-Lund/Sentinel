#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
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

/* Protects IPC start/stop lifecycle operations. */
static pthread_mutex_t g_lifecycle_lock =
    PTHREAD_MUTEX_INITIALIZER;

    static pthread_mutex_t g_reader_lock = PTHREAD_MUTEX_INITIALIZER;

static WorkerProcess *s_workers = NULL;
static int g_count = 0;
static pthread_mutex_t *g_lock = NULL;

/* Per-worker state: accessed only by the IPC reader thread,
   or by ipc_stop() after the reader thread has joined. */
static char  g_buf[IPC_MAX][IPC_LINE_LEN];
static int   g_len[IPC_MAX];
static int   g_fd[IPC_MAX];
static pid_t g_pid[IPC_MAX];
static int   g_id[IPC_MAX];
static int   g_eof[IPC_MAX];

/* Write the buffered line to the logger. */
static void emit_line(int i)
{
    g_buf[i][g_len[i]] = '\0';

    if (g_len[i] > 0 && g_buf[i][g_len[i] - 1] == '\r')
        g_buf[i][--g_len[i]] = '\0';

    if (g_len[i] > 0)
        log_event("OUT", "Worker %d: %s", g_id[i], g_buf[i]);

    g_len[i] = 0;
}

/* Process incoming bytes and assemble complete lines. */
static void feed(int i, const char *data, int n)
{
    for (int k = 0; k < n; k++)
    {
        if (data[k] == '\n')
        {
            emit_line(i);
        }
        else
        {
            if (g_len[i] >= IPC_LINE_LEN - 1)
                emit_line(i);

            g_buf[i][g_len[i]++] = data[k];
        }
    }
}

/*
 * Read output from one worker.
 *
 * Returns:
 *   > 0 : number of bytes read
 *     0 : no data, EOF, or error
 */
static int read_worker_unlocked(int i)
{
    char chunk[256];

    int fd;
    int id;
    pid_t pid;

    /* Step 1: snapshot worker information. */
    pthread_mutex_lock(g_lock);

    fd = s_workers[i].pipe_read_fd;
    id = s_workers[i].worker_id;
    pid = s_workers[i].pid;

    pthread_mutex_unlock(g_lock);

    /* Step 2: detect a restarted worker or replaced pipe. */
    if (fd != g_fd[i] || pid != g_pid[i])
    {
        emit_line(i);

        g_fd[i] = fd;
        g_pid[i] = pid;
        g_eof[i] = 0;
    }

    g_id[i] = id;

    if (fd < 0 || g_eof[i])
        return 0;

    int got = 0;
    int ended = 0;

    const char *problem = NULL;

    /*
     * Step 3: validate the descriptor and PID again while
     * holding the worker mutex. This prevents the process
     * manager from closing the descriptor during the read.
     */
    pthread_mutex_lock(g_lock);

    if (s_workers[i].pipe_read_fd == fd &&
        s_workers[i].pid == pid)
    {
        /* Ensure the read descriptor is nonblocking. */
        int flags = fcntl(fd, F_GETFL);

        if (flags < 0)
        {
            problem = "fcntl(F_GETFL) failed";
            ended = 1;
        }
        else if (!(flags & O_NONBLOCK) &&
                 fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            problem = "fcntl(F_SETFL) failed";
            ended = 1;
        }

        if (!ended)
        {
            struct pollfd p;

            p.fd = fd;
            p.events = POLLIN;
            p.revents = 0;

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
                    ssize_t r = read(fd, chunk, sizeof chunk);

                    if (r > 0)
                    {
                        got = (int)r;
                    }
                    else if (r == 0)
                    {
                        ended = 1;
                    }
                    else if (errno != EINTR && errno != EAGAIN &&
                             errno != EWOULDBLOCK)
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
    }

    pthread_mutex_unlock(g_lock);

    /* Step 4: process received data outside worker mutex. */
    if (got > 0)
        feed(i, chunk, got);

    /* Step 5: handle EOF and pipe errors. */
    if (ended)
    {
        emit_line(i);

        g_eof[i] = 1;

        if (problem)
        {
            log_event("WARN",
                      "Worker %d: %s, stopped reading its output",
                      id, problem);
        }
        else
        {
            log_event("INFO",
                      "Worker %d: output pipe closed (EOF)",
                      id);
        }
    }

    return got;
}


static int read_worker(int i)
{
    pthread_mutex_lock(&g_reader_lock);

    int result = read_worker_unlocked(i);

    pthread_mutex_unlock(&g_reader_lock);

    return result;
}
/* Background IPC reader thread. */
static void *ipc_loop(void *arg)
{
    (void)arg;

    while (!atomic_load(&g_stop))
    {
        for (int i = 0; i < g_count; i++)
        {
            for (int round = 0;
                 round < 8 && read_worker(i) > 0;
                 round++)
            {
                /* Drain currently available output. */
            }
        }

        /* Sleep for 200 ms, checking stop periodically. */
        for (int k = 0; k < 2 && !atomic_load(&g_stop); k++)
            usleep(100000);
    }

    return NULL;
}

/* Start the IPC reader. */
int ipc_start(WorkerProcess *workers,
              int count,
              pthread_mutex_t *lock)
{
    pthread_mutex_lock(&g_lifecycle_lock);

    if (atomic_load(&g_started) ||
        !workers ||
        !lock ||
        count < 1 ||
        count > IPC_MAX)
    {
        pthread_mutex_unlock(&g_lifecycle_lock);
        return -1;
    }

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
    {
        s_workers = NULL;
        g_count = 0;
        g_lock = NULL;

        pthread_mutex_unlock(&g_lifecycle_lock);
        return -1;
    }

    atomic_store(&g_started, true);

    log_event("INFO", "IPC reader started (%d workers)", count);

    pthread_mutex_unlock(&g_lifecycle_lock);

    return 0;
}

/*
 * Stop the IPC reader.
 *
 * IMPORTANT:
 * Workers should stop producing output before this function
 * performs its final drain. Keep pipe read descriptors open
 * until this function returns.
 */
int ipc_drain_worker(WorkerProcess *worker)
{
    if (worker == NULL || !atomic_load(&g_started))
        return -1;

    pthread_mutex_lock(&g_reader_lock);

    int index = -1;

    for (int i = 0; i < g_count; i++)
    {
        if (&s_workers[i] == worker)
        {
            index = i;
            break;
        }
    }

    if (index < 0)
    {
        pthread_mutex_unlock(&g_reader_lock);
        return -1;
    }

    pthread_mutex_lock(g_lock);

    int fd = worker->pipe_read_fd;
    int id = worker->worker_id;

    pthread_mutex_unlock(g_lock);

    if (fd < 0)
    {
        pthread_mutex_unlock(&g_reader_lock);
        return 0;
    }

    int flags = fcntl(fd, F_GETFL);

    if (flags < 0 ||
        (!(flags & O_NONBLOCK) &&
         fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0))
    {
        pthread_mutex_unlock(&g_reader_lock);
        return -1;
    }

    char chunk[256];
    int eof = 0;

    while (!eof)
    {
        ssize_t n = read(fd, chunk, sizeof(chunk));

        if (n > 0)
        {
            g_id[index] = id;
            feed(index, chunk, (int)n);
        }
        else if (n == 0)
        {
            eof = 1;
        }
        else if (errno == EINTR)
        {
            continue;
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            /*
             * No more data is currently available.
             * The process manager calls this after waitpid,
             * so the worker should already have exited.
             */
            break;
        }
        else
        {
            pthread_mutex_unlock(&g_reader_lock);
            return -1;
        }
    }

    /* Flush any final partial line. */
    g_id[index] = id;
    emit_line(index);

    /* Close the descriptor only if it is still the same pipe. */
    pthread_mutex_lock(g_lock);

    if (worker->pipe_read_fd == fd)
    {
        close(fd);
        worker->pipe_read_fd = -1;
    }

    pthread_mutex_unlock(g_lock);

    g_fd[index] = -1;
    g_pid[index] = 0;
    g_eof[index] = 1;

    pthread_mutex_unlock(&g_reader_lock);

    return 0;
}


void ipc_stop(void)
{
    pthread_mutex_lock(&g_lifecycle_lock);

    if (!atomic_load(&g_started))
    {
        pthread_mutex_unlock(&g_lifecycle_lock);
        return;
    }

    atomic_store(&g_stop, true);

    /* Wait for the background reader to finish. */
    pthread_join(g_thread, NULL);

    /*
     * Final drain:
     * Read all currently available output and flush
     * incomplete lines.
     */
    for (int i = 0; i < g_count; i++)
    {
        while (read_worker(i) > 0)
        {
            /* Continue until no more data is available. */
        }

        emit_line(i);
    }

    log_event("INFO", "IPC reader stopped");

    atomic_store(&g_started, false);

    pthread_mutex_unlock(&g_lifecycle_lock);
}