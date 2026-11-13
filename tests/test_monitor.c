#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "logger.h"
#include "monitor.h"
#include "test_util.h"

#define LOG "tests/test.log"

int main(void)
{
    remove(LOG);

    /* logger: argument check, repeated init, double close */
    CHECK(log_init(NULL) == -1);
    CHECK(log_init(LOG) == 0);
    CHECK(log_init(LOG) == 0);          /* repeated call is safe */
    log_event("INFO", "hello %d", 42);
    log_close();
    log_close();                        /* second close is safe */
    log_event("INFO", "after close");   /* must not crash or write */
    CHECK(count_in_file(LOG, "hello 42") == 1);
    CHECK(count_in_file(LOG, "after close") == 0);

    /* monitor: argument validation */
    proc_stats_t bad;
    memset(&bad, 0, sizeof bad);
    CHECK(monitor_read(0, &bad) == -1);
    CHECK(monitor_read(-5, &bad) == -1);
    CHECK(monitor_read(getpid(), NULL) == -1);

    /* monitor: a busy child */
    pid_t p = fork();
    CHECK(p >= 0);
    if (p == 0) { for (;;); }

    proc_stats_t s;
    memset(&s, 0, sizeof s);
    sleep(1);
    CHECK(monitor_read(p, &s) == 0);
    CHECK(s.pid == p);
    CHECK(s.cpu_percent == 0.0);        /* first reading is the baseline */
    CHECK(s.state == 'R' || s.state == 'S');

    sleep(2);
    CHECK(monitor_read(p, &s) == 0);
    printf("busy child cpu=%.1f%%\n", s.cpu_percent);
    CHECK(s.cpu_percent > 0.0 && s.cpu_percent < 150.0);

    /* tick counter going backwards must not produce a huge value */
    s.prev_ticks = s.utime + s.stime + 100000UL;
    sleep(1);
    CHECK(monitor_read(p, &s) == 0);
    CHECK(s.cpu_percent >= 0.0 && s.cpu_percent < 1.0);

    /* a different pid starts a new baseline */
    CHECK(monitor_read(getpid(), &s) == 0);
    CHECK(s.pid == getpid());
    CHECK(s.cpu_percent == 0.0);

    CHECK(kill(p, SIGKILL) == 0);
    CHECK(waitpid(p, NULL, 0) == p);
    CHECK(monitor_read(p, &s) == -1);   /* process is gone */

    printf("test_monitor: ALL PASSED\n");
    return 0;
} 
