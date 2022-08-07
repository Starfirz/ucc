CC=gcc
CFLAGS=-O2 -g -Wall -Wextra -Wpedantic

.PHONY: all clean
all: client server

client: tcp_thr.c
	${CC} ${CFLAGS} -o $@ $^

server: tcp_thr.c
	${CC} ${CFLAGS} -o $@ $^

clean:
	rm client server

