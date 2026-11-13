# Sentinel

Sentinel is a Linux process supervisor for a configurable set of worker
processes. It monitors worker state, captures worker stdout through IPC pipes,
logs per-worker process statistics, and restarts workers that exit unexpectedly.
At startup it also runs FCFS, SJF, and Round Robin simulations for a synthetic
task per worker, logging the calculated results. This scheduler simulation
does not control or change Linux kernel scheduling policy.

## Build

Requirements: GCC, GNU Make, and Linux/POSIX process and pthread APIs.

```sh
make clean
make
```

## Run

Run with the default five workers, or pass a worker count from 1 to 10:

```sh
./bin/sentinel
./bin/sentinel 3
```

Sentinel runs 12 monitoring cycles by default and intentionally simulates
Worker 1 crashes during the run to exercise recovery. Worker output and
supervisor events, CPU status, and scheduler results are written to
`sentinel.log`. Press Ctrl+C for an early, graceful shutdown.

## Test

Build and run the scheduler, monitor, monitor-thread, and IPC tests with:

```sh
make test
```