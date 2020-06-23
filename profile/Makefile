CC = gcc
LNK = gcc

CFLAGS_GIO = `pkg-config --cflags gio-2.0`
CFLAGS_GIO_UNIX = `pkg-config --cflags gio-unix-2.0`
CFLAGS = $(CFLAGS_GIO) $(CFLAGS_GIO_UNIX) -c -g -O0

LIBDIR = /usr/lib/x86_64-linux-gnu/
LIBS = `pkg-config --libs gio-2.0` -lbluetooth

all: profile_server profile_client

# link using explicit arguments for testing, alternatively simply use
# the flags as provided by pkg-config, i.e. something like that:
# gcc 'pkg-config --libs gio-2.0'

rpm_control_profile: rpm_control_profile.o
	$(LNK) -L$(LIBDIR) rpm_control_profile.o -o rpm_control_profile $(LIBS)

profile_server: profile_server.o
	$(LNK) -L$(LIBDIR) profile_server.o -o profile_server $(LIBS)

profile_client: profile_client.o
	$(LNK) -L$(LIBDIR) profile_client.o -o profile_client $(LIBS)

# compile using explicit arguments for testing, alternatively simply use
# the flags as provided by pkg-config, i.e. something like that:
# gcc 'pkg-config --cflags gio-2.0'

profile_server.o: profile_server.c profile_record.h
	$(CC) ${CFLAGS} profile_server.c

profile_client.o: profile_client.c profile_record.h
	$(CC) ${CFLAGS} profile_client.c

clean:
	rm -rf profile_server profile_server.o \
	profile_client profile_client.o
