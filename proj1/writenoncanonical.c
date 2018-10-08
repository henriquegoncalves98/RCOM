/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

struct termios oldtio,newtio;
int timeout = 3;
int retries = 0;
int resend = 0;
int received = 0;

void llopen(int fd);
void llclose(int fd);

static void alarmHandler()
{
  printf("Nothing received");
  retries++;
  resend = 1;
  received = 0;
}


int main(int argc, char** argv)
{
  int fd, res;
  int i, sum = 0, speed = 0;
  
  if ( (argc < 2) || 
      ((strcmp("/dev/ttyS0", argv[1])!=0) && 
        (strcmp("/dev/ttyS1", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }

	fd = open(argv[1], O_RDWR | O_NOCTTY );
  if (fd <0) {
    perror(argv[1]); exit(-1); 
  }

  signal(SIGALRM, alarmHandler);

	llopen(fd);


	

  


  llclose(fd);

  close(fd);
  return 0;
}



void llopen(int fd)
{
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
  newtio.c_cc[VMIN]     = 5;   /* blocking read until 1 char received */
/* 
  VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
  leitura do(s) pr�ximo(s) caracter(es)
*/



  tcflush(fd, TCIOFLUSH);

  if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");

  int res;
  char SETarray[5];
	SETarray[0] = 0x7E;
	SETarray[1] = 0x03;
	SETarray[2] = 0x03;
	SETarray[3] = SETarray[1]^SETarray[2];
	SETarray[4] = SETarray[0];

	size_t SETarraySize = sizeof(SETarray)/sizeof(SETarray[0]);
	
	write(fd, SETarray, SETarraySize); 

  alarm(timeout);

  char buf[255];

  while( !received && (retries != 3) ) {

    if( resend ) { 
      write(fd, SETarray, SETarraySize);
      alarm(timeout);
    }

    else {
      res = read(fd, buf, 255);

      if(buf[res-1] == 0x7e) {
        received = 1;
        retries = 0;
        resend = 0;
      }
    }
  }

  if(retries == 3) {
    printf("didnt work");
  }


}


void llclose(int fd) {

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
}
