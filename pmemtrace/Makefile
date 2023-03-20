CC = gcc
CFLAGS = -Wall -Wextra -pthread
SRC = pmemtrace.c
OUT = pmemtrace

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OUT)

