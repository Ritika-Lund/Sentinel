#include <stdio.h>
#include "../src/scheduler.h"

int main(void)
{
    /* Scenario 1: idle gap (P2 arrives long after P1 finishes) */
    Task a[2];
    task_init(&a[0], 1, 0, 0, 2);
    task_init(&a[1], 2, 0, 10, 3);
    schedule_fcfs(a, 2);  print_results("S1 FCFS (idle gap)", a, 2);
    schedule_sjf(a, 2);   print_results("S1 SJF (idle gap)", a, 2);
    schedule_rr(a, 2, 2); print_results("S1 RR q=2 (idle gap)", a, 2);

    /* Scenario 2: three identical tasks, all arrive at 0 */
    Task b[3];
    task_init(&b[0], 1, 0, 0, 3);
    task_init(&b[1], 2, 0, 0, 3);
    task_init(&b[2], 3, 0, 0, 3);
    schedule_fcfs(b, 3);  print_results("S2 FCFS (ties)", b, 3);
    schedule_sjf(b, 3);   print_results("S2 SJF (ties)", b, 3);
    schedule_rr(b, 3, 5); print_results("S2 RR q=5 (quantum > burst)", b, 3);
    schedule_rr(b, 3, 1); print_results("S2 RR q=1", b, 3);

    /* Scenario 3: single task */
    Task c[1];
    task_init(&c[0], 1, 0, 0, 5);
    schedule_rr(c, 1, 2); print_results("S3 RR q=2 (single task)", c, 1);

    return 0;
}
