CC = gcc
LNK = gcc

CFLAGS = `pkg-config --cflags gio-2.0` -c -g -O0
LIBDIR = /usr/lib/x86_64-linux-gnu/
LIBS = `pkg-config --libs gio-2.0` -lbluetooth

all: mesh_advertizer

# link using explicit arguments for testing, alternatively simply use
# the flags as provided by pkg-config, i.e. something like that:
# gcc 'pkg-config --libs gio-2.0'

mesh_advertizer: mesh_advertizer.o
	$(LNK) -L$(LIBDIR) mesh_advertizer.o -o mesh_advertizer $(LIBS)

# compile using explicit arguments for testing, alternatively simply use
# the flags as provided by pkg-config, i.e. something like that:
# gcc 'pkg-config --cflags gio-2.0'

mesh_advertizer.o: mesh_advertizer.c
	$(CC) ${CFLAGS} mesh_advertizer.c

clean:
	rm -rf mesh_advertizer mesh_advertizer.o
