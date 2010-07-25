CC = gcc
#CFLAGS = -Wall -DSOCK_VERBOSE -Ddebug -g3
CFLAGS = -Wall -Wextra -DSOCK_VERBOSE -c -o
#LDFLAGS = -lpthread
OBJ = bin/main.o bin/networking.o bin/notify.o bin/proto.o

all: bin bin/main

bin/%.o: src/%.c
	$(CC) $(CFLAGS) $@ $<

bin/main: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

bin:
	mkdir -p bin

.PHONY: clean

clean:
	rm -r bin
