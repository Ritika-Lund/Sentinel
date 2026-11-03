#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Worker ID is required.\n");
        return 1;
    }

    int worker_id = atoi(argv[1]);

    printf("[WORKER %d] Started. PID: %d\n",
           worker_id, getpid());

    int counter = 1;

    while (1)
    {
        printf("[WORKER %d] Working... Task %d\n",
               worker_id, counter++);

        sleep(2);
    }

    return 0;
}