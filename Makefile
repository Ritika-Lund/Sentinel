CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

SRC = $(wildcard src/*.c)
TEST_DIR = bin/tests
TEST_CFLAGS = $(CFLAGS) -Isrc
TEST_BINS = $(TEST_DIR)/test_scheduler $(TEST_DIR)/test_edge_cases \
	$(TEST_DIR)/test_monitor $(TEST_DIR)/test_monitor_thread \
	$(TEST_DIR)/test_ipc

all: bin/sentinel bin/worker

bin:
	mkdir -p bin

bin/sentinel: $(SRC) | bin
	$(CC) $(CFLAGS) -o $@ $(SRC)

bin/worker: workers/worker.c | bin
	$(CC) $(CFLAGS) -o $@ workers/worker.c

$(TEST_DIR): | bin
	mkdir -p $@

$(TEST_DIR)/test_scheduler: tests/test_scheduler.c src/scheduler.c src/scheduler.h tests/test_util.h | $(TEST_DIR)
	$(CC) $(TEST_CFLAGS) -o $@ tests/test_scheduler.c src/scheduler.c

$(TEST_DIR)/test_edge_cases: tests/test_edge_cases.c src/scheduler.c src/scheduler.h | $(TEST_DIR)
	$(CC) $(TEST_CFLAGS) -o $@ tests/test_edge_cases.c src/scheduler.c

$(TEST_DIR)/test_monitor: tests/test_monitor.c src/monitor.c src/logger.c src/monitor.h src/logger.h tests/test_util.h | $(TEST_DIR)
	$(CC) $(TEST_CFLAGS) -o $@ tests/test_monitor.c src/monitor.c src/logger.c

$(TEST_DIR)/test_monitor_thread: tests/test_monitor_thread.c src/monitor_thread.c src/monitor.c src/logger.c src/monitor_thread.h src/monitor.h src/logger.h tests/test_util.h | $(TEST_DIR)
	$(CC) $(TEST_CFLAGS) -o $@ tests/test_monitor_thread.c src/monitor_thread.c src/monitor.c src/logger.c

$(TEST_DIR)/test_ipc: tests/test_ipc.c src/ipc.c src/logger.c src/ipc.h src/process.h src/logger.h tests/test_util.h | $(TEST_DIR)
	$(CC) $(TEST_CFLAGS) -o $@ tests/test_ipc.c src/ipc.c src/logger.c

test: $(TEST_BINS)
	@set -e; for test in $(TEST_BINS); do echo "Running $$test"; ./$$test; done

clean:
	rm -rf bin

.PHONY: all clean test