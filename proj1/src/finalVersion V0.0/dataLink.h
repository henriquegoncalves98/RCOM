#ifndef DATALINK_H
#define DATALINK_H

// header contents
#include "application.h"
#include "alarm.h"
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <strings.h>


#define BAUDRATE 			B38400

#define FALSE 				0
#define TRUE 				1

#define FLAG 				0x7E
#define A 					0x03
#define SET_C 				0x03
#define UA_C 				0x07
#define SET_BCC1 			A^SET_C
#define UA_BCC1 			A^UA_C
#define C0					0x00
#define C1					0x40
#define RR0_C				0x05
#define RR1_C				0x85
#define REJ0_C				0x01
#define REJ1_C				0x81
#define DISC_C				0x0B
#define C_DATA              0x01
#define C_START             0x02
#define C_END               0x03
#define T1                  0x00
#define T2                  0x01


#define ESCAPE			    0x7D
#define ESCAPE_FLAG         0x5E
#define ESCAPE_ESCAPE       0x5D


typedef struct {
  unsigned char port[20];
  int baudRate;
  int timeout;
  int NUM_RETRIES;
  int frameExpected;
  int bytesForEachPacket;
  int numTotalPackets;
} LinkLayer;

extern LinkLayer *linkLayer;

int llopen(int fd, int mode);
int llwrite(int fd, unsigned char * buffer, int length);
int llread(int fd, unsigned char * buffer);
void llclose(int fd);


#endif
