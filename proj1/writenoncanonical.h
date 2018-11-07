#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>


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

#define bcc1ErrorPercentage 0
#define bcc2ErrorPercentage 0

enum state_machine {START, FLAG_RCV, A_RCV, C_RCV, BCC1_RCV, ESCAPING, DONE};

int llopen(int fd);

void sendSUFrame(int fd, unsigned char c);

int caughtSUFrame(int fd, unsigned char CFlag);

void caughtUA(enum state_machine *state, unsigned char *c);

void checkACK(enum state_machine *state, unsigned char *c, unsigned char *ctrl);

int llwrite(int fd, unsigned char * buffer, int length);

unsigned char calculoBCC2(unsigned char *mensagem, int size);

unsigned char *stuffingBCC2(unsigned char BCC2, int *sizeBCC2);

unsigned char *readFile(unsigned char *fileName, off_t *fileSize);

void sendControlFrame(int fd, unsigned char state, char* fileSizeBuf, unsigned char *fileName);

/*
* Splits the message from the file selected in packets.
* Application layer
*/
unsigned char *cutMessage(unsigned char *message, off_t *indice, int *sizeDataPacket, off_t FileSize);

/*
* It appends the packet header to the begginning of data packets
* Application layer
*/
unsigned char *packetHeader(unsigned char *message, int *sizeDataPacket, off_t FileSize);

unsigned char *BCC1changer(unsigned char *packet, int sizePacket);

unsigned char *BCC2changer(unsigned char *packet, int sizePacket);

void llclose(int fd);
