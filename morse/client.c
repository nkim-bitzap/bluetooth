/******************************************************************************/
/* File: client.c
   Author: N.Kim
   Abstract: Client part of the Bluetooth Morse tutorial */
/******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/rfcomm.h>

#define PACKET_LENGTH 8

//-----------------------------------------------------------------------------
//
int main(int argc, char **argv)
{
  struct sockaddr_rc addr = {0};
  int s, status;
  char *message = "hello!";
  char dest[18] = {0};

  if (argc < 2) {
    fprintf(stderr, "usage: %s <bt_addr>\n", argv[0]);
    return 1;
  }

  strncpy(dest, argv[1], 18);
  s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

  // set the connection parameters (who to connect to)
  addr.rc_family = AF_BLUETOOTH;
  addr.rc_bdaddr = *BDADDR_ANY;
  addr.rc_channel = 27;  /* test */

  str2ba(dest, &addr.rc_bdaddr);

  /* connect to them morse server that does decoding */
  status = connect(s, (struct sockaddr *)&addr, sizeof(addr));

  printf("Establishing connection...");

  if (status == 0) {
    printf("ok\n");
    printf("Press <SPACE + ENTER> to quit, or message to send:\n");

    while (1)  {
      char c = getchar();

      /* reserve 8 bytes to encode messages to be sent */
      char data[PACKET_LENGTH] = "        ";

      if (c == 0x20) {
        /* spacebar as loop exit, stretch to 8 bytes */
	printf("Terminating processing loop...");
	status = write(s, "goodbye!", PACKET_LENGTH);

       	if (status < 0) printf("failed\n");
	else printf("ok\n");
        break;
      }
      else {
	/* catch lower case alpha's and single digits only */
	if ((c >= '0' && c <= '9') || (c >= 'a') && (c <= 'z')) {
          printf("  sending '%c'...", c);
	  data[0] = c;

          status = write(s, data, PACKET_LENGTH);

          if (status < 0) printf("failed\n");
          else printf("ok\n");
        }
      }
    }
  }
  else printf("failed\n");

  close(s);
  return 0;
}

