CC = gcc
CFLAGS = -Wall -Wextra -c -o
#LDFLAGS = -lpthread
OBJ = bin/main.o bin/server.o bin/proto.o bin/log.o

all: bin bin/main test

bin/%.o: src/%.c
	$(CC) $(CFLAGS) $@ $<

bin/main: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

bin:
	mkdir -p bin

test:
	sh test/main

.PHONY: clean

clean:
	rm -r bin
