CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
SRC = $(wildcard src/*.c)

all: bin/sentinel

bin/sentinel: $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(SRC)

clean:
	rm -rf bin
