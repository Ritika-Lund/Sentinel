#include <stdio.h>
#include "../src/scheduler.h"

int main(void)
{
    Task tasks[4];
    task_init(&tasks[0], 1, 0, 0, 5);
    task_init(&tasks[1], 2, 0, 1, 3);
    task_init(&tasks[2], 3, 0, 2, 8);
    task_init(&tasks[3], 4, 0, 3, 6);

    schedule_fcfs(tasks, 4);
    print_results("FCFS", tasks, 4);

    schedule_sjf(tasks, 4);
    print_results("SJF", tasks, 4);

    return 0;
}
