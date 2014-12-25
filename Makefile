CFLAGS=-Wall -DDEBUG -g -og
LDFLAGS=-g

all: main

main: main.o client_pool.o handler.o cgi_handler.o

client_pool: client_pool.o

clean:
	rm -f *.o server

cleano:
	rm -f *.o
