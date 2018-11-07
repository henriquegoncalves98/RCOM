#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FLAG 				0x7E
#define A 					0x03
#define SET_C 				0x03
#define UA_C				0x07
#define SET_BCC1 			A^SET_C
#define UA_BCC1 			A^UA_C
#define C0                  0x00
#define C1                  0x40
#define RR_C0               0x05
#define RR_C1               0x85 
#define REJ_C0              0x01
#define REJ_C1              0x81 
#define DISC_C              0x0B
#define C_START             0x02
#define C_END               0x03

#define ESCAPE			    0x7D
#define ESCAPE_FLAG         0x5E
#define ESCAPE_ESCAPE       0x5D


enum state_machine {START, FLAG_RCV, A_RCV, C_RCV, BCC1_RCV, ESCAPING, DONE};

typedef struct {
   unsigned char    *fileName;
   int              fileNameLength;
   int 				fileSize;
} Message;


void llopen(int fd);

int llread(int fd, unsigned char * buffer);

int caughtSUFrame(int fd, unsigned char CFlag);

int hasBCC2();

void sendAcknowlegment(int fd, unsigned char c);

void getFileInfo(unsigned char *startFrame, int startFrameSize, Message *message);

int hasFinishedReceiving(unsigned char *packet, int packetSize, unsigned char *startFrame, int startFrameSize);

void getPacketInfo(unsigned char *fileData, int *fileDataSize, unsigned char *packet, int packetSize);

void makeNewFile(Message message, unsigned char *fileData, int fileDataSize);

void llclose(int fd);
