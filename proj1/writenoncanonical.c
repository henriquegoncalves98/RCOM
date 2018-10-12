/*Non-Canonical Input Processing*/
#include "writenoncanonical.h"

#define BAUDRATE 			B38400
#define MODEMDEVICE 		"/dev/ttyS1"
#define _POSIX_SOURCE 		1 /* POSIX compliant source */
#define FALSE 				0
#define TRUE 				1

#define FLAG 				0x7E
#define A 					0x03
#define SET_C 				0x03
#define SET_BCC1 			A^SET_C


volatile int STOP=FALSE;

struct termios oldtio,newtio;
int timeout = 3;
int retries = 0;
int resend = 0;
int received = 0;


static void alarmHandler()
{
  printf("Nothing received");
  retries++;
  resend = 1;
  received = 0;
}


int main(int argc, char** argv)
{
int fd;

	//PORT CONFIGURATION AND OPENING PORTS
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
	//END OF PORT CONFIGURATION AND OPENING PORTS

	//signal(SIGALRM, alarmHandler);

	llopen(fd);

	/* RESTO DO PROTOCOLO (INFO FRAMES ETC.) */

	llclose(fd);
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
	newtio.c_cc[VMIN]     = 0;   /* blocking read until 1 char received */


	tcflush(fd, TCIOFLUSH);

	if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	printf("New termios structure set\n");

	sendSET(fd);

	
	
	/*
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
	*/
}

void sendSET(int fd) {

	char SETarray[5];
	SETarray[0] = FLAG;
	SETarray[1] = A;
	SETarray[2] = SET_C;
	SETarray[3] = SET_BCC1;
	SETarray[4] = FLAG;

	size_t SETarraySize = sizeof(SETarray)/sizeof(SETarray[0]);

	write(fd, SETarray, SETarraySize); 
}


void llclose(int fd) {

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
  close(fd);
}
