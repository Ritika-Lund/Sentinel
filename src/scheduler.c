#include <stdio.h>
#include <stdlib.h>
#include "scheduler.h"

void task_init(Task *t, int id, pid_t pid, int arrival, int burst)
{
    t->id = id;
    t->pid = pid;
    t->arrival = arrival;
    t->burst = burst;
    t->remaining = burst;
    t->start = -1;
    t->finish = 0;
    t->waiting = 0;
    t->turnaround = 0;
    t->done = 0;
}

void scheduler_reset(Task *tasks, int n)
{
    for (int i = 0; i < n; i++) {
        tasks[i].remaining = tasks[i].burst;
        tasks[i].start = -1;
        tasks[i].finish = 0;
        tasks[i].waiting = 0;
        tasks[i].turnaround = 0;
        tasks[i].done = 0;
    }
}

static int cmp_arrival(const void *a, const void *b)
{
    const Task *x = a;
    const Task *y = b;
    if (x->arrival != y->arrival)
        return x->arrival - y->arrival;
    return x->id - y->id;  
}


void schedule_fcfs(Task *tasks, int n)
{
    scheduler_reset(tasks, n);
    qsort(tasks, n, sizeof(Task), cmp_arrival);

    int time = 0;
    for (int i = 0; i < n; i++) {
        if (time < tasks[i].arrival)
            time = tasks[i].arrival;      

        tasks[i].start = time;
        time += tasks[i].burst;
        tasks[i].finish = time;
        tasks[i].turnaround = tasks[i].finish - tasks[i].arrival;
        tasks[i].waiting = tasks[i].turnaround - tasks[i].burst;
        tasks[i].remaining = 0;
        tasks[i].done = 1;
    }
}

void print_results(const char *algo_name, const Task *tasks, int n)
{
    double total_wait = 0, total_tat = 0;

    printf("\n=== %s ===\n", algo_name);
    printf("%-5s %-8s %-6s %-7s %-7s %-8s %-10s\n",
           "Task", "Arrival", "Burst", "Start", "Finish", "Waiting", "Turnaround");

    for (int i = 0; i < n; i++) {
        printf("P%-4d %-8d %-6d %-7d %-7d %-8d %-10d\n",
               tasks[i].id, tasks[i].arrival, tasks[i].burst,
               tasks[i].start, tasks[i].finish,
               tasks[i].waiting, tasks[i].turnaround);
        total_wait += tasks[i].waiting;
        total_tat  += tasks[i].turnaround;
    }
    printf("Average waiting time:    %.2f\n", total_wait / n);
    printf("Average turnaround time: %.2f\n", total_tat / n);
}

/* SJF (non-preemptive): whenever the CPU is free, pick the task that
   has already arrived and has the smallest burst, and run it to the end.
   Ties: earlier arrival first, then lower id. */
void schedule_sjf(Task *tasks, int n)
{
    scheduler_reset(tasks, n);

    int time = 0;
    int completed = 0;

    while (completed < n) {
        int pick = -1;

        for (int i = 0; i < n; i++) {
            if (tasks[i].done || tasks[i].arrival > time)
                continue;
            if (pick == -1 ||
                tasks[i].burst < tasks[pick].burst ||
                (tasks[i].burst == tasks[pick].burst &&
                 (tasks[i].arrival < tasks[pick].arrival ||
                  (tasks[i].arrival == tasks[pick].arrival &&
                   tasks[i].id < tasks[pick].id))))
                pick = i;
        }

        if (pick == -1) {
            /* nothing has arrived yet: jump to the earliest arrival */
            int next = -1;
            for (int i = 0; i < n; i++) {
                if (!tasks[i].done && (next == -1 || tasks[i].arrival < next))
                    next = tasks[i].arrival;
            }
            time = next;
            continue;
        }

        tasks[pick].start = time;
        time += tasks[pick].burst;
        tasks[pick].finish = time;
        tasks[pick].turnaround = tasks[pick].finish - tasks[pick].arrival;
        tasks[pick].waiting = tasks[pick].turnaround - tasks[pick].burst;
        tasks[pick].remaining = 0;
        tasks[pick].done = 1;
        completed++;
    }
}
