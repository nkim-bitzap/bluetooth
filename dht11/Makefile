CC=gcc
LNK=gcc

all: sensor

sensor: sensor.o
	$(LNK) sensor.o -o sensor -Wl,-rpath,$(PIGPIO_LIB_DIR),\
	-L$(PIGPIO_LIB_DIR),-lrt,-lpigpio

sensor.o: sensor.c
	$(CC) -I$(PIGPIO_INCLUDE_DIR) -c sensor.c

.PHONY: clean

clean:
	rm -f sensor sensor.o
