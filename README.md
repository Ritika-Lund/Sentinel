# SENTINEL – Linux Process Supervisor with CPU Scheduling and Automatic Recovery

SENTINEL is a Linux-based process supervision system written in C. It manages worker processes, monitors their status and CPU usage, demonstrates CPU scheduling algorithms, captures worker output through pipes, records system events, and attempts to recover workers that terminate unexpectedly.

> **Platform note:** SENTINEL uses Linux-specific facilities such as `fork()`, `exec()`, signals, pipes, and the `/proc` filesystem. It is designed to run on Linux. On a Windows computer, use **WSL 2 with Ubuntu** (instructions below). Native Windows compilation and execution are not supported by the current implementation.

## Features

- **Process management:** Creates and controls worker processes using Linux system calls.
- **Automatic recovery:** Detects unexpected worker termination and attempts to restart the worker up to a configured maximum.
- **CPU scheduling simulations:** Implements First Come First Serve (FCFS), Shortest Job First (SJF), and Round Robin (RR).
- **Process monitoring:** Reads process information from Linux `/proc`, including process state and CPU-time information.
- **Multithreading:** Runs monitoring and IPC-reading activities in background POSIX threads.
- **Inter-process communication:** Uses Linux pipes to capture output from worker processes.
- **Logging:** Records monitoring information, worker output, and system events in `sentinel.log`.

> **Scheduling note:** FCFS, SJF, and Round Robin are simulations that calculate scheduling results. They do not change or replace the Linux kernel's actual CPU scheduler.

## Technologies Used

- C
- Ubuntu Linux
- GCC
- GNU Make
- Linux process system calls: `fork()`, `exec()`, `waitpid()`, and signals
- POSIX threads (`pthread`)
- Linux pipes
- POSIX mutexes
- Linux `/proc` filesystem
- Git and GitHub

## Project Structure

```text
SENTINEL/
├── config/
├── docs/
├── src/
│   ├── main.c
│   ├── process.c
│   ├── process.h
│   ├── recovery.c
│   ├── recovery.h
│   ├── monitor.c
│   ├── monitor.h
│   ├── monitor_thread.c
│   ├── monitor_thread.h
│   ├── logger.c
│   ├── logger.h
│   ├── ipc.c
│   ├── ipc.h
│   ├── scheduler.c
│   └── scheduler.h
├── workers/
│   └── worker.c
├── tests/
├── Makefile
└── README.md
```

## Requirements

### Linux

Use Ubuntu Linux with GCC and GNU Make installed.

Install the required build tools:

```bash
sudo apt update
sudo apt install build-essential git -y
```

### Windows

The current SENTINEL source code is Linux-specific, so it will not run directly in Command Prompt or PowerShell as a native Windows program. To run it on Windows, install **Windows Subsystem for Linux (WSL 2)** and an Ubuntu distribution.

1. Open **PowerShell as Administrator**.
2. Install WSL and Ubuntu:

   ```powershell
   wsl --install -d Ubuntu
   ```

3. Restart Windows if prompted.
4. Open **Ubuntu** from the Start menu.
5. Complete the first-time Ubuntu setup by creating a Linux username and password.
6. In the Ubuntu terminal, install the build tools:

   ```bash
   sudo apt update
   sudo apt install build-essential git -y
   ```

7. Continue with the Linux clone, build, and run instructions below, using the Ubuntu terminal.

If WSL is already installed, open Ubuntu from the Start menu and continue from step 6.

## Get the Source Code

Clone the repository and enter the project directory:

```bash
git clone https://github.com/Ritika-Lund/Sentinel.git
cd Sentinel
```

Run these commands inside the Ubuntu/Linux terminal, including when using WSL.

## Build Instructions

Clean any previous build and compile the project:

```bash
make clean
make
```

After a successful build, the executables should be available at:

```text
bin/
├── sentinel
└── worker
```

## Run SENTINEL

Run with seven workers:

```bash
./bin/sentinel 7
```

The worker-count argument is optional. If omitted, the program uses the default configured in `main.c`. The current implementation supports up to **10 workers**.

For example, to run with five workers:

```bash
./bin/sentinel 5
```

## View the Log File

SENTINEL writes log messages to `sentinel.log` in the directory from which the program is run, provided logging is initialized successfully.

Display the latest log entries:

```bash
tail sentinel.log
```

Follow new log entries in real time:

```bash
tail -f sentinel.log
```

Press `Ctrl+C` to stop following the log. This stops the log display only; it does not stop SENTINEL in the other terminal.

The log can include worker status and CPU usage, worker output collected through IPC, background-thread messages, and shutdown events.

## Test Automatic Recovery

The current program includes a worker-crash simulation. During its monitoring cycles, it simulates the termination of Worker 1. SENTINEL should detect the unexpected termination and attempt to restart the worker. The restart counter increases with each attempt. When the configured maximum is reached, the worker is marked as `FAILED`.

The other workers should be stopped during shutdown.

## Scheduling Output

During execution, SENTINEL prints results for FCFS, SJF, and Round Robin. The results include task arrival time, burst time, start time, finish time, waiting time, and turnaround time, along with average waiting and turnaround times.

These results are calculated by the scheduler simulator and are separate from live Linux worker-process execution.

## Stop the Program

Press `Ctrl+C` in the terminal running SENTINEL to request shutdown. The program should stop its worker processes and background threads and record shutdown events.

## Team Contributions

| Team member | Responsibility | Main contribution |
|---|---|---|
| Deepak Lal | Process management and automatic recovery | Worker creation and control, process-state handling, crash detection, and restart attempts |
| Ritika Lund | CPU scheduling | FCFS, SJF, and Round Robin scheduling simulations and performance calculations |
| Nancy Chawla | Monitoring, IPC, and logging | `/proc`-based monitoring, background threads, pipe-based worker-output collection, and event logging |

## Operating Systems Concepts Demonstrated

- Process creation and management
- Process states and signals
- CPU scheduling algorithms
- Process monitoring through `/proc`
- Inter-process communication using pipes
- POSIX multithreading
- Mutex-based synchronization
- Failure detection and automatic recovery
- Event logging

## Notes

- Run commands from the project root directory.
- Run the application from the Ubuntu/Linux environment, including when using WSL.
- The worker executable is expected at `./bin/worker` when SENTINEL starts workers.
- CPU readings can be `0.0%` for workers that perform little CPU-intensive work during the measurement interval.
- Scheduling algorithms are educational simulations; they do not set Linux process priorities or control kernel scheduling.

## License

No license information was provided with the project. Add a `LICENSE` file and update this section if the project is distributed under a specific license.