CC = gcc
CFLAGS = -Wall -Wextra -I../headers
LDFLAGS = 

SRCS = krx_matching.c
TARGET = transaction_checker

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean