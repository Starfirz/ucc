CC=mpicc
CFLAGS=-O2 -g -Wall
LDFLAGS=-lucp -luct -lucs -lucm

.PHONY: all clean
all: ucx_pingpong

ucx_pingpong: ucx.c mpi.c
        ${CC} ${CFLAGS} -o $@ $^ ${LDFLAGS}

clean:
        rm ucx_pingpong