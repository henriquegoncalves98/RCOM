#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

enum state_machine {START, FLAG_RCV, A_RCV, C_RCV, BCC1_RCV, DONE};

int llopen(int fd);

void sendSET(int fd);

void caughtUA(enum state_machine *state, unsigned char *c);

void checkACK(enum state_machine *state, unsigned char *c);

int llwrite(int fd, char * buffer, int length);

unsigned char calculoBCC2(unsigned char *mensagem, int size);

unsigned char *stuffingBCC2(unsigned char BCC2, int *sizeBCC2);

void llclose(int fd);
