#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum state_machine {START, FLAG_RCV, A_RCV, C_RCV, BCC1_RCV, DONE};

void llopen(int fd);

int caughtSET(int fd);

void sendUA(int fd);

void llclose(int fd);