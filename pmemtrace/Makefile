CC = gcc
CFLAGS = -O3 -Wall -Wextra -pthread -march=native
SRC = pmemtrace.c
OUT = pmemtrace

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OUT)

