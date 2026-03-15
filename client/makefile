CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11
LDFLAGS = -pthread

TARGET = client
SRC = client.c
HDR = client.h

all: $(TARGET)

$(TARGET): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
