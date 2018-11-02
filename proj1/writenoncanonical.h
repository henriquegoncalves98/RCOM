#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

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
#define DISC				0x0B
#define C_START             0x02
#define T1                  0x00


#define ESCAPE			    0x7D
#define ESCAPE_FLAG         0x5E
#define ESCAPE_ESCAPE       0x5D

enum state_machine {START, FLAG_RCV, A_RCV, C_RCV, BCC1_RCV, DONE};

int llopen(int fd);

void sendSET(int fd);

void caughtUA(enum state_machine *state, unsigned char *c);

void checkACK(enum state_machine *state, unsigned char *c, unsigned char *ctrl);

int llwrite(int fd, char * buffer, int length);

unsigned char calculoBCC2(unsigned char *mensagem, int size);

unsigned char *stuffingBCC2(unsigned char BCC2, int *sizeBCC2);

unsigned char *readFile(unsigned char *fileName, off_t *fileSize);

unsigned char *makeStartFrame(off_t fileSize, unsigned char *fileName, int fileNameSize);

void llclose(int fd);
