/******************************************************************************/
/* File: server.c
   Author: N.Kim
   Abstract: Server part of the Bluetooth Morse tutorial */
/******************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>

#define PACKET_LENGTH 8

char decode_table[128] = {0};

/*****************************************************************************/

void setup_decoding_table(void)
{
  /* beat it manually. This is waste of space, but elegance is not our
     main concern here. I am just going to paste a pre-fabricated
     table here */

  /* zeroes represent dots and ones represent dashes. Because the code
     is variable in length, we reserve 3 MSB bits to encode the length
     of a particular character */

  decode_table['a'] = 0x41; // 01000001
  decode_table['b'] = 0x88; // 10001000
  decode_table['c'] = 0x8A; // 10001010
  decode_table['d'] = 0x64; // 01100100
  decode_table['e'] = 0x20; // 00100000
  decode_table['f'] = 0x82; // 10000010
  decode_table['g'] = 0x66; // 01100110
  decode_table['h'] = 0x80; // 10000000
  decode_table['i'] = 0x40; // 01000000
  decode_table['j'] = 0x87; // 10000111
  decode_table['k'] = 0x65; // 01100101
  decode_table['l'] = 0x84; // 10000100
  decode_table['m'] = 0x43; // 01000011
  decode_table['n'] = 0x42; // 01000010
  decode_table['o'] = 0x67; // 01100111
  decode_table['p'] = 0x86; // 10000110
  decode_table['q'] = 0x8D; // 10001101
  decode_table['r'] = 0x62; // 01100010
  decode_table['s'] = 0x60; // 01100000
  decode_table['t'] = 0x21; // 00100001
  decode_table['u'] = 0x61; // 01100001
  decode_table['v'] = 0x81; // 10000001
  decode_table['w'] = 0x63; // 01100011
  decode_table['x'] = 0x89; // 10001001
  decode_table['y'] = 0x8B; // 10001011
  decode_table['z'] = 0x8C; // 10001100
  decode_table['0'] = 0xBF; // 10111111
  decode_table['1'] = 0xAF; // 10101111
  decode_table['2'] = 0xA7; // 10100111
  decode_table['3'] = 0xA3; // 10100011
  decode_table['4'] = 0xA1; // 10100001
  decode_table['5'] = 0xA0; // 10100000
  decode_table['6'] = 0xB0; // 10110000
  decode_table['7'] = 0xB8; // 10111000
  decode_table['8'] = 0xBC; // 10111100
  decode_table['9'] = 0xBE; // 10111110
}

/*****************************************************************************/

unsigned int get_length_mask(int length)
{
  /* get a mask that needs to be applied on a particular entry in the
     decoding table to extract significant bits. Alphas and numbers
     have at most 5 morse digits */
  switch(length)
  {
    case 1: return 0x1;
    case 2: return 0x3;
    case 3: return 0x7;
    case 4: return 0xF;
    case 5: return 0x1F;
    default: break;
  }

  return 0;
}

/*****************************************************************************/

void print_chain(const char c)
{
  /* turn a character into a sequence of morse digits, i.e. dots/dashes */
  if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z'))
  {
    /* get the pattern from the decoding table */
    char data = decode_table[c];

    /* extract the length */
    unsigned int length = (data >> 5) & 0x7;

    /* get the length mask */
    unsigned int mask = get_length_mask(length);

    /* apply the mask to extract the morse representation for 'c' */
    unsigned char chain = data & mask;

    /* now just print dots and dashes */
    printf("(%c) ", c);

    for (int i = 0; i < length; ++i)
    {
      unsigned char tmp = (chain << PACKET_LENGTH - length + i);
      unsigned char sign = tmp >> PACKET_LENGTH - 1;

      if (sign == 0) printf(".");
      else printf("-");
    }

    printf("\n");
  }
}

/*****************************************************************************/

int main(int argc, char **argv)
{
  struct sockaddr_l2 loc_addr = {0};
  struct sockaddr_l2 rem_addr = {0};

  char buf[64] = {0};

  int s, client, bytes_read;
  socklen_t opt = sizeof(rem_addr);

  /* this is similar to the client's implementation with the additional
     functionality of listening */
  s = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);

  /* bind the socket to port 0x1001 of the first available bluetooth
     adapter */
  loc_addr.l2_family = AF_BLUETOOTH;
  loc_addr.l2_bdaddr = *BDADDR_ANY;
  loc_addr.l2_psm = htobs(0x1001);

  bind(s, (struct sockaddr*)&loc_addr, sizeof(loc_addr));

  printf("Start listening...\n");

  /* put socket into listening state */
  listen(s, 1);

  /* accept only one connection at a time */
  client = accept(s, (struct sockaddr*)&rem_addr, &opt);

  ba2str(&rem_addr.l2_bdaddr, buf);
  printf("Connected to %s\n", buf);

  /* now done connecting, lets do morse-ing */
  memset(buf, 0, sizeof(buf));
  setup_decoding_table();

  while(1)
  {
    /* always expect to read 8 bytes from the client, bail if this
       is not the case */
    bytes_read = read(client, buf, sizeof(buf));

    if (8 == bytes_read)
    {
      if (0 == strcmp(buf, "goodbye!")) break;
      else print_chain(buf[0]);
    }
    else
    {
      printf("(bad character)\n");
      break;
    }
  }

  /* housekeeping */
  close(client);
  close(s);

  printf("Done listening\n");
  return 0;
}

