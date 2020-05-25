CC = gcc
LNK = gcc

CFLAGS = `pkg-config --cflags gio-2.0` -c
LIBDIR = /usr/lib/x86_64-linux-gnu/
LIBS = `pkg-config --libs gio-2.0` -lbluetooth

all: server client

# link using explicit arguments for testing, alternatively simply use
# the flags as provided by pkg-config, i.e. something like that:
# gcc 'pkg-config --libs gio-2.0'

client: client.o
	$(LNK) -L$(LIBDIR) client.o -o client $(LIBS)

server: server.o
	$(LNK) -L$(LIBDIR) server.o -o server $(LIBS)


# compile using explicit arguments for testing, alternatively simply use
# the flags as provided by pkg-config, i.e. something like that:
# gcc 'pkg-config --cflags gio-2.0'

client.o: client.c
	$(CC) ${CFLAGS} client.c

server.o: server.c
	$(CC) ${CFLAGS} server.c

clean:
	rm -rf client server client.o server.o
