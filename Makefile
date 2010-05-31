CC = gcc
#CFLAGS = -Wall -DSOCK_VERBOSE -Ddebug -g3
CFLAGS = -Wall -DSOCK_VERBOSE
#CFLAGS = -Wall
#LDFLAGS = -lpthread
OBJ = bin/main.o bin/networking.o bin/notify.o bin/proto.o

all: bin/main

bin/%.o: src/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

bin/main: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f bin/*
