CC = gcc
CFLAGS = -Wall -Wextra
SRC = tracer.c
OUT = tracer

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OUT)

