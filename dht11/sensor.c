/*****************************************************************************/
/* File: sensor.c                                                            */
/* Author: NKim                                                              */
/* Abstract: Definition of functions dealing with sensor interfacing         */
/*****************************************************************************/

#include <pigpio.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>

/* NOTE, my sensor does not seem to work properly on the 3.3V power rail
   (red), thus, use the 5V GPIO power supply. The data port itself (yellow)
   is in turn not restricted */
#define DHT_GPIO_PORT 4

/* this is something i am not entirely sure about, but this seems to work ok
   for my configuration. This corresponds to the number of pulses to be
   received after the initial 18 ms delay has been performed */
#define DHT_INIT_RESPONSE_LENGTH 3

/* each bit is represented by 2 pulses, additionally reserve 3 fields for the
   pulses at the beginning of the initialization sequence (see manual) */
#define DHT_BUFFER_LENGTH 80 + DHT_INIT_RESPONSE_LENGTH

struct init_response_info {
  char tick_buf[DHT_BUFFER_LENGTH];
  int tick_count;
  int tick_index;
  int running;
  int finished;
  int error;
} respinfo = { {0}, 0, 0, 0, 0, 0 };

/* global, but thats ok for this tutorial */
int sigint_detected = 0;

/*****************************************************************************/

void on_sigint_receive(int signo)
{
  if (signo == SIGINT) sigint_detected = 1;
}

/*****************************************************************************/

void on_init_response_change(int gpio, int level, uint32_t ticks) {
  /* this is the callback we have installed for the sampler. This means,
     whenever the pin (connected to the sensor) level changes, we get
     notified. We also receive the number of ticks that have passed
     since the last pin change. Thus, record them in order to be able to
     check later whether we are dealing with ones or zeroes */
  if (gpio != DHT_GPIO_PORT || level < 0 || level > 1) return;

  if (respinfo.finished == 1) return;

  if (respinfo.tick_index < DHT_BUFFER_LENGTH) {
    if (respinfo.running == 0) {
      respinfo.running = 1;
      respinfo.tick_count = ticks;
      return;
    }

    const int length = ticks - respinfo.tick_count;

    /* record reasonable pulse lengths only, according to the manual,
       the highest value is somewhere in the region of 70us (which
       means 1 btw) */

    if (length > 10 && length < 100) {
      respinfo.tick_buf[respinfo.tick_index] = length;
      respinfo.tick_count = ticks;
      respinfo.tick_index++;
    }
    else {
      respinfo.error = 1;
      return;
    }
  }
  else {
    respinfo.running = 0;
    respinfo.finished = 1;
  }
}

/*****************************************************************************/

void clear_response_info(void) {
  respinfo.running = 0;
  respinfo.finished = 0;
  respinfo.tick_count = 0;
  respinfo.tick_index = 0;
  respinfo.error = 0;
}

/*****************************************************************************/

int decode_data_bit(int pulse_length) {
  /* long pulses are interpreted as 1 and short ones as 1. Concrete
     average values are somewhere in the range of 25us for 0 and 70us
     for 1 */

  if (pulse_length >= 15 && pulse_length <= 35) return 0;
  else if (pulse_length >= 60 && pulse_length <= 80) return 1;
  else return -1;
}

//-----------------------------------------------------------------------------

int init_sensor_reading() {
  /* require only one GPIO port for now, the dht11 sensor is mapped to port 4.
     The data reading requires a predefined signal pattern (see dht11 manual),
     thus, we need to write to the port first. Wait 1 sec for the port to
     become 'stable' */
  gpioSetMode(DHT_GPIO_PORT, PI_OUTPUT);
  gpioSetPullUpDown(DHT_GPIO_PORT, PI_PUD_DOWN);
  gpioSleep(PI_TIME_RELATIVE, 1, 0);

  /* pull on low for min. 18msec. After this the manual becomes barely
     understandable. Especially, its unclear whether the pulse of 20-40 usec
     is to be interpreted as output (i.e. to be sent from the MCU) or input
     (incoming acknowledgement from the sensor). Since i keep receiving short
     spikes, i assume the second case */
  gpioWrite(DHT_GPIO_PORT, 0);
  gpioSleep(PI_TIME_RELATIVE, 0, 18000);

  gpioWrite(DHT_GPIO_PORT, 1);
  gpioSleep(PI_TIME_RELATIVE, 0, 30);
}

//-----------------------------------------------------------------------------

int read_data(void) {

  gpioSetMode(DHT_GPIO_PORT, PI_INPUT);

  clear_response_info();

  /* specify the handler for the sampling and give the reading process 5000
     usec to complete. This should be plenty */
  gpioSetAlertFunc(DHT_GPIO_PORT, on_init_response_change);
  gpioSleep(PI_TIME_RELATIVE, 0, 5000);
  gpioSetAlertFunc(DHT_GPIO_PORT, NULL);

  if (respinfo.running || !respinfo.finished) {
    fprintf(stderr, "Timeout occured while reading data\n");
    return -1;
  }

  /* check the length of the first 3 elements, these correspond to the init
     response from the sensor. The first spike is described to be somewhere
     in the range of 20 and 40 usec. Allow additional margins of 10 usec */
  if (respinfo.tick_buf[0] < 10 || respinfo.tick_buf[0] > 50) {
    fprintf(stderr, "Bad length (first response pulse)\n");
    return -1;
  }

  /* second and third pulses are specified to be in the range of 80 usec, we
     allow a margin of 15 usec here */
  if (respinfo.tick_buf[1] < 65 || respinfo.tick_buf[1] > 95 ||
      respinfo.tick_buf[2] < 65 || respinfo.tick_buf[2] > 95)
  {
    fprintf(stderr, "Bad length (second/third response pulse)\n");
    return -1;
  }

  // now start inspecting received data. Each bit is encoded by 2 pulses,
  // with the first being of constant length (50 usec) and the second puls
  // being variable (20-30 usec for 0 and 70 usec for 1)
  for (int i = DHT_INIT_RESPONSE_LENGTH; i < DHT_BUFFER_LENGTH; i += 2) {
    if (respinfo.tick_buf[i] < 40 || respinfo.tick_buf[i] > 60) {
      fprintf(stderr, "Bad header pulse length %d at position %d\n",
              respinfo.tick_buf[i], i);
      return -1;
    }

    const int good_low =
      respinfo.tick_buf[i+1] >= 15 && respinfo.tick_buf[i+1] <= 35;

    const int good_high =
      respinfo.tick_buf[i+1] >= 55 && respinfo.tick_buf[i+1] <= 85;

    if (!(good_low || good_high)) {
      fprintf(stderr, "Bad data pulse length %d at position %d\n",
              respinfo.tick_buf[i+1], i);
      return -1;
    }
  }

  return 0;
}

//-----------------------------------------------------------------------------

int decode_data(unsigned char *temp_high,
                unsigned char *temp_low,
                unsigned char *hum_high,
                unsigned char *hum_low,
                unsigned char *parity)
{
  /* again, two pulses (i.e. array fields) for each data bit, thus, if we
     want to extract 8 bits, we first need to skip the first 3 indices (init
     response pattern) and then scan 16 positions of the buffer */
  int start_index = DHT_INIT_RESPONSE_LENGTH;
  int end_index = start_index + 16;

  for (int i = start_index, j = 0; i < end_index; i += 2, ++j) {
    int bit = decode_data_bit(respinfo.tick_buf[i+1]);
    *hum_high = *hum_high | bit << (7 - j);
  }

  start_index += 16;
  end_index += 16;

  /* according to the manual, it is sufficient to read out the high byte for
     the humidity, for the parity check we still extract the low byte too */
  for (int i = start_index, j = 0; i < end_index; i += 2, ++j) {
    int bit = decode_data_bit(respinfo.tick_buf[i+1]);
    *hum_low = *hum_low | bit << (7 - j);
  }

  start_index += 16;
  end_index += 16;

  for (int i = start_index, j = 0; i < end_index; i += 2, j++) {
    int bit = decode_data_bit(respinfo.tick_buf[i+1]);
    *temp_high = *temp_high | bit << (7 - j);
  }

  start_index += 16;
  end_index += 16;

  /* the same applies to the temperature as well, the lower byte only encodes
     the fractional value of the temperature, so this part can be skipped too
     for a quick and lazy implementation */
  for (int i = start_index, j = 0; i < end_index; i += 2, j++) {
    int bit = decode_data_bit(respinfo.tick_buf[i+1]);
    *temp_low = *temp_low | bit << (7 - j);
  }

  start_index += 16;
  end_index += 16;

  for (int i = start_index, j = 0; i <= end_index; i += 2, ++j) {
    int bit = decode_data_bit(respinfo.tick_buf[i+1]);
    *parity = *parity | bit << (7 - j);
  }
}

//-----------------------------------------------------------------------------

int main(int argc, char **argv)
{
  printf("Starting temperature/humidity monitoring\n");

  /* first things first, do this */
  if (gpioInitialise() < 0)
  {
    fprintf(stderr, "Failed GPIO initialization\n");
    return -1;
  }

  if (gpioSetPullUpDown(DHT_GPIO_PORT, PI_PUD_UP) != 0)
  {
    fprintf(stderr, "Failed setting internal pull-up\n");
    goto terminate;
  }

  /* install a signal handler to terminate the main loop */
  signal(SIGINT, on_sigint_receive);

  /* top level processing is very simple: init, read, decode,
     print. Repeat until tired */
  while (!sigint_detected)
  {
    init_sensor_reading();

    if (read_data() != -1)
    {
      unsigned char temp_high = 0;
      unsigned char temp_low = 0;
      unsigned char hum_high = 0;
      unsigned char hum_low = 0;
      unsigned char parity = 0;

      decode_data(
        &temp_high,
        &temp_low,
        &hum_high,
        &hum_low,
        &parity);

      const int byte_sum = temp_high + temp_low + hum_high + hum_low;

      printf("Sensor data:\n");
      printf("  temperature high: %d\n", temp_high);
      printf("  temperature low: %d\n", temp_low);
      printf("  humidity high: %d\n", hum_high);
      printf("  humidity low: %d\n", hum_low);
      printf("  parity: %d\n", parity);

      printf("data status: ");

      if (byte_sum != parity)
        printf("invalid\n");
      else printf("ok\n");
    }

    /* perform data acquisition in economy mode. I.e. we read sensor data
       every 2 seconds */
    gpioSleep(PI_TIME_RELATIVE, 2, 0);
  }

terminate:
  gpioTerminate();
  printf("Monitoring done\n");
  return 0;
}
