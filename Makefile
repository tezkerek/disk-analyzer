CC=gcc
CFLAGS=-I.
DEPS = ipc.h utils.h
OBJ = ipc.o utils.c

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

build: daemon client

daemon: daemon.o $(OBJ)
	$(CC) -o dad $^

client: client.o $(OBJ)
	$(CC) -o da $^ $(CFLAGS)
