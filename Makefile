CFLAGS=-Wall -DDEBUG -g -og
LDFLAGS=-g

all: lisod

lisod: lisod.o client_pool.o handler.o ssl_handler.o cgi_handler.o -lssl

client_pool: client_pool.o

clean:
	rm -f *.o lisod
