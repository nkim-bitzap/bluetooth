CC = gcc
LNK = gcc

CFLAGS = `pkg-config --cflags gio-2.0` -c -g -O0
LIBDIR = /usr/lib/x86_64-linux-gnu/
LIBS = `pkg-config --libs gio-2.0` -lbluetooth

all: advertizer

# link using explicit arguments for testing, alternatively simply use
# the flags as provided by pkg-config, i.e. something like that:
# gcc 'pkg-config --libs gio-2.0'

advertizer: advertizer.o
	$(LNK) -L$(LIBDIR) advertizer.o -o advertizer $(LIBS)

# compile using explicit arguments for testing, alternatively simply use
# the flags as provided by pkg-config, i.e. something like that:
# gcc 'pkg-config --cflags gio-2.0'

advertizer.o: advertizer.c
	$(CC) ${CFLAGS} advertizer.c

clean:
	rm -rf advertizer advertizer.o
