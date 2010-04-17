CC = gcc
CFLAGS = -Wall -DSOCK_VERBOSE -g3 -s
LDFLAGS =
OBJ = bin/main.o bin/networking.o bin/notify.o bin/proto.o

all: bin/main

bin/%.o: src/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

bin/main: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS)-o $@ $^ $(LIBS)

clean:
	rm -f bin/*
