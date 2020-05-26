CC = gcc
LNK = gcc

CFLAGS = `pkg-config --cflags gio-2.0` -c
LIBDIR = /usr/lib/x86_64-linux-gnu/
LIBS = `pkg-config --libs gio-2.0` -lbluetooth

all: connector

# link using explicit arguments for testing, alternatively simply use
# the flags as provided by pkg-config, i.e. something like that:
# gcc 'pkg-config --libs gio-2.0'

connector: connector.o
	$(LNK) -L$(LIBDIR) connector.o -o connector $(LIBS)

# compile using explicit arguments for testing, alternatively simply use
# the flags as provided by pkg-config, i.e. something like that:
# gcc 'pkg-config --cflags gio-2.0'

connector.o: connector.c
	$(CC) ${CFLAGS} connector.c

clean:
	rm -rf connector connector.o
