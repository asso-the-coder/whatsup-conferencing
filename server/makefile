CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11
LDFLAGS = -pthread

TARGET = server
SRC = server.c
HDR = server.h

all: $(TARGET)

$(TARGET): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
