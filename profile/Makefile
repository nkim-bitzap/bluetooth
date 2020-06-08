CC = gcc
LNK = gcc

CFLAGS = `pkg-config --cflags gio-2.0` -c -g -O0
LIBDIR = /usr/lib/x86_64-linux-gnu/
LIBS = `pkg-config --libs gio-2.0` -lbluetooth

all: profile_server profile_client

# link using explicit arguments for testing, alternatively simply use
# the flags as provided by pkg-config, i.e. something like that:
# gcc 'pkg-config --libs gio-2.0'

profile_server: profile_server.o
	$(LNK) -L$(LIBDIR) profile_server.o -o profile_server $(LIBS)

profile_client: profile_client.o
	$(LNK) -L$(LIBDIR) profile_client.o -o profile_client $(LIBS)

# compile using explicit arguments for testing, alternatively simply use
# the flags as provided by pkg-config, i.e. something like that:
# gcc 'pkg-config --cflags gio-2.0'

profile_server.o: profile_server.c
	$(CC) ${CFLAGS} profile_server.c

profile_client.o: profile_client.c
	$(CC) ${CFLAGS} profile_client.c

clean:
	rm -rf profile_server profile_server.o \
	profile_client profile_client.o
