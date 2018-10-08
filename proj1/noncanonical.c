/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

struct termios oldtio,newtio;

void llopen(int fd);
void llclose(int fd);

int main(int argc, char** argv)
{
  int fd, c, res;
  char buf[255];

  if ( (argc < 2) || 
        ((strcmp("/dev/ttyS0", argv[1])!=0) && 
        (strcmp("/dev/ttyS1", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  
    
  fd = open(argv[1], O_RDWR | O_NOCTTY );
  if (fd <0) {
    perror(argv[1]); exit(-1); 
  }

    
  llopen(fd);


  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
  */



  char UAarray[5];
	UAarray[0] = 0x7E;
	UAarray[1] = 0x03;
	UAarray[2] = 0x07;
	UAarray[3] = UAarray[1]^UAarray[2];
	UAarray[4] = UAarray[0];

	size_t UAarraySize = sizeof(UAarray)/sizeof(UAarray[0]);
	
	write(fd, UAarray, UAarraySize);



  /* 
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no gui�o 
  */



  llclose(fd);

  close(fd);

  return 0;
}

void llopen(int fd) {

  if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
  newtio.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */

  tcflush(fd, TCIOFLUSH);

  if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");


  int res, j;
  char buf[255];
  
  while (STOP==FALSE) {           /* loop for input */    
    res = read(fd, buf, 255);     /* returns after 5 chars have been input */
    for(j=0; j<res; j++)
        printf("%X ", buf[j]);
    printf("\n");
    if (buf[res-1] == 0x7e) 
      STOP=TRUE;
  }
}


void llclose(int fd) {
  
  tcsetattr(fd,TCSANOW,&oldtio);
}

