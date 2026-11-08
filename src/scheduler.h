#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <sys/types.h>

#define MAX_TASKS 64

typedef struct {
    int   id;         
    pid_t pid;         
    int   arrival;     
    int   burst;       
    int   remaining;   
    int   start;       
    int   finish;      
    int   waiting;     
    int   turnaround;  
    int   done;        
} Task;

void task_init(Task *t, int id, pid_t pid, int arrival, int burst);
void scheduler_reset(Task *tasks, int n);
void schedule_fcfs(Task *tasks, int n);
void schedule_sjf(Task *tasks, int n);
void schedule_rr(Task *tasks ,int n, int qunatum);
void print_results(const char *algo_name, const Task *tasks, int n);

#endif
