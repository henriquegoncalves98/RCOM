#ifndef APPLICATION_H
#define APPLICATION_H

#include "dataLink.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>



typedef struct {
  int fd;               //Door file descriptor
  int status;           //0 - Transmiter || 1 - Receiver
  unsigned char *fileName;
}  ApplicationLayer;

typedef struct {
   unsigned char      *fileName;
   int                fileNameLength;
   int 				         fileSize;
} Message;

extern ApplicationLayer *applicationLayer;

enum state_machine {START, FLAG_RCV, A_RCV, C_RCV, BCC1_RCV, ESCAPING, DONE};

void startApplication(unsigned char* port, int mode, int maxBytesPerPacket, int retries, int timeout, unsigned char *fileName);

void startTransmiterApp();

void startReceiverApp();

int caughtSUFrame(int fd, unsigned char CFlag);

int hasBCC2();

void sendAcknowlegment(int fd, unsigned char c);

void getFileInfo(unsigned char *startFrame, int startFrameSize, Message *message);

int hasFinishedReceiving(unsigned char *packet, int packetSize, unsigned char *startFrame, int startFrameSize);

void getPacketInfo(unsigned char *fileData, int *fileDataSize, unsigned char *packet, int packetSize);

void makeNewFile(Message message, unsigned char *fileData, int fileDataSize);

void sendSUFrame(int fd, unsigned char c);

int caughtSUFrame(int fd, unsigned char CFlag);

void caughtUA(enum state_machine *state, unsigned char *c);

void checkACK(enum state_machine *state, unsigned char *c, unsigned char *ctrl);

unsigned char calculoBCC2(unsigned char *mensagem, int size);

unsigned char *stuffingBCC2(unsigned char BCC2, int *sizeBCC2);

unsigned char *readFile(unsigned char *fileName, off_t *fileSize);

void sendControlFrame(int fd, unsigned char state, char* fileSizeBuf, unsigned char *fileName);

unsigned char *cutMessage(unsigned char *message, off_t *indice, int *sizeDataPacket, off_t FileSize);

unsigned char *packetHeader(unsigned char *message, int *sizeDataPacket, off_t FileSize);

#endif
