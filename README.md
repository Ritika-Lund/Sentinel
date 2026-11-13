# SENTINEL – Linux Process Supervisor

SENTINEL is a Linux program written in C. It starts worker processes, watches them, and restarts a worker if it stops unexpectedly. It also shows how three CPU scheduling algorithms work and saves everything it does in a log file.

> **Note:** SENTINEL only runs on Linux. On Windows, use WSL 2 with Ubuntu (steps below).

## Features

- Creates and controls worker processes
- Restarts a worker if it stops unexpectedly (up to a set limit)
- CPU scheduling simulations: FCFS, SJF, and Round Robin
- Monitors worker status and CPU usage using `/proc`
- Uses background threads (pthreads) for monitoring and reading worker output
- Collects worker output using pipes
- Saves messages and events in `sentinel.log`

> The scheduling algorithms are simulations. They only calculate results and do not change the real Linux CPU scheduler.

## Technologies Used

- C, GCC, GNU Make
- Ubuntu Linux
- `fork()`, `exec()`, `waitpid()`, and signals
- POSIX threads and mutexes
- Linux pipes
- Linux `/proc` filesystem
- Git and GitHub

## Project Structure

```text
SENTINEL/
├── config/
├── docs/
├── src/
│   ├── main.c
│   ├── process.c / process.h
│   ├── recovery.c / recovery.h
│   ├── monitor.c / monitor.h
│   ├── monitor_thread.c / monitor_thread.h
│   ├── logger.c / logger.h
│   ├── ipc.c / ipc.h
│   └── scheduler.c / scheduler.h
├── workers/
│   └── worker.c
├── tests/
├── Makefile
└── README.md
```

## Requirements

### On Linux

Install the build tools:

```bash
sudo apt update
sudo apt install build-essential git -y
```

### On Windows (using WSL 2)

1. Open **PowerShell as Administrator** and run:

   ```powershell
   wsl --install -d Ubuntu
   ```

2. Restart Windows if asked.
3. Open **Ubuntu** from the Start menu and create a username and password.
4. In the Ubuntu terminal, install the build tools:

   ```bash
   sudo apt update
   sudo apt install build-essential git -y
   ```

5. Follow the steps below in the Ubuntu terminal.

If WSL is already installed, just open Ubuntu and start from step 4.

## How to Get and Build the Project

Run these commands in the Ubuntu/Linux terminal:

```bash
git clone https://github.com/Ritika-Lund/Sentinel.git
cd Sentinel
make clean
make
```

After a successful build, the `bin/` folder will contain two programs: `sentinel` and `worker`.

## How to Run

Run with 7 workers:

```bash
./bin/sentinel 7
```

The number of workers is optional. If you leave it out, the default value from `main.c` is used. The maximum is 10 workers.

Example with 5 workers:

```bash
./bin/sentinel 5
```

## Log File

SENTINEL saves its messages in `sentinel.log` in the folder where you run it.

Show the latest lines:

```bash
tail sentinel.log
```

Watch new lines live (use a second terminal):

```bash
tail -f sentinel.log
```

Press `Ctrl+C` to stop watching the log. This does not stop SENTINEL.

The log can show worker status, CPU usage, worker output, thread messages, and shutdown events.

## Testing Automatic Recovery

The program includes a crash simulation for Worker 1:

1. SENTINEL simulates Worker 1 stopping.
2. SENTINEL detects it and tries to restart it.
3. The restart counter goes up each time.
4. When the maximum is reached, Worker 1 is marked as `FAILED`.

The other workers are stopped when the program shuts down.

## Scheduling Output

While running, SENTINEL prints results for FCFS, SJF, and Round Robin. For each task it shows arrival time, burst time, start time, finish time, waiting time, and turnaround time. It also shows the average waiting time and average turnaround time.

## Stopping the Program

Press `Ctrl+C` in the terminal where SENTINEL is running. It will stop the workers and background threads and record the shutdown in the log.

## Operating Systems Concepts Used

- Process creation and management
- Process states and signals
- CPU scheduling algorithms
- Process monitoring with `/proc`
- Inter-process communication with pipes
- Multithreading
- Mutex synchronization
- Failure detection and recovery
- Event logging

## Team Contributions

| Team member | Responsibility |
|---|---|
| Deepak Lal | Process management and automatic recovery |
| Ritika Lund | CPU scheduling (FCFS, SJF, Round Robin) |
| Nancy Chawla | Monitoring, pipes (IPC), and logging |

## Notes

- Run all commands from the project's main folder.
- The worker program must be at `./bin/worker`.
- CPU usage can show `0.0%` if a worker does very little work.

## License

No license has been added yet. If you add one, put a `LICENSE` file in the project and update this section.