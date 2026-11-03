CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

SRC = $(wildcard src/*.c)

all: bin/sentinel bin/worker

bin:
	mkdir -p bin

bin/sentinel: $(SRC) | bin
	$(CC) $(CFLAGS) -o $@ $(SRC)

bin/worker: workers/worker.c | bin
	$(CC) $(CFLAGS) -o $@ workers/worker.c

clean:
	rm -rf bin

.PHONY: all clean