CC = gcc
CFLAGS = -Wall -Wextra -g

TARGET = bin/sentinel
SOURCE = src/main.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	mkdir -p bin
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

clean:
	rm -rf bin
