#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "logger.h"
#include "monitor.h"

int main(void)
{
    log_init("tests/test.log");

    pid_t p = fork();
    if (p == 0) { for (;;); }          /* child burns CPU */

    proc_stats_t s;
    memset(&s, 0, sizeof s);
    for (int i = 0; i < 4; i++)
    {
        sleep(1);
        if (monitor_read(p, &s) == 0)
        {
            printf("pid=%d state=%c cpu=%.1f%%\n", (int)p, s.state, s.cpu_percent);
            log_event("INFO", "pid=%d state=%c cpu=%.1f", (int)p, s.state, s.cpu_percent);
        }
    }

    kill(p, SIGKILL);
    waitpid(p, NULL, 0);
    printf("after kill, monitor_read returns %d (expect -1)\n", monitor_read(p, &s));
    log_close();
    return 0;
}
