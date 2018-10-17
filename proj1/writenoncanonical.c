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
#define UA_C 				0x07
#define SET_BCC1 			A^SET_C
#define UA_BCC1 			A^UA_C


volatile int STOP=FALSE;

struct termios oldtio, newtio;
int timeout = 3;
int retries = 0;
int resend = FALSE;
int received = FALSE;


static void alarmHandler(int sig)
{
  printf("Nothing received\n");
  retries++;
  resend = TRUE;
  received = FALSE;
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

	(void) signal(SIGALRM, alarmHandler);

	if( llopen(fd) == 0 ) {
		printf("Failed to make a connection to the receiver \n");
		return -1;
	}

	/* RESTO DO PROTOCOLO (INFO FRAMES ETC.) */

	llclose(fd);
	return 0;
}



int llopen(int fd)
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

	newtio.c_cc[VTIME]    = 30;   /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 0;   /* blocking read until 1 char received */


	tcflush(fd, TCIOFLUSH);

	if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	printf("New termios structure set\n");



	sendSET(fd);

	alarm(timeout);


	enum state_machine state = START;
	unsigned char c;

	while( !received && (retries != 3) ) {

		if( resend ) { printf("110\n"); 
			sendSET(fd);
			alarm(timeout);
			resend = FALSE;
			state = START;
		}
		else {
			read(fd, &c, 1);
			caughtUA(&state, &c);

			if(state == DONE)
			{
				printf("UA received");
				received = TRUE;
			}
		}
	}

	if(retries == 3) {
		printf("didnt work");
		return 0;
	}

	return 1;
}

void sendSET(int fd) {

	char SETarray[5];
	SETarray[0] = FLAG;
	SETarray[1] = 0x08;
	SETarray[2] = SET_C;
	SETarray[3] = SET_BCC1;
	SETarray[4] = FLAG;

	size_t SETarraySize = sizeof(SETarray)/sizeof(SETarray[0]);

	write(fd, SETarray, SETarraySize); 
}

void caughtUA(enum state_machine *state, unsigned char *c) {

	while(*state != DONE) {

		switch(*state) {
			case(START):
				if(*c == FLAG)
					*state = FLAG_RCV;
				break;
			
			case(FLAG_RCV):
				if(*c == A)
					*state = A_RCV;
				else {
					if(*c == FLAG)
						break;
					else
						*state = START;
				}
				break;

			case(A_RCV):
				if(*c == UA_C)
					*state = C_RCV;
				else {
					if(*c == FLAG)
						*state = FLAG_RCV;
					else
						*state = START;
				}
				break;
			
			case(C_RCV):
				if(*c == UA_BCC1)
					*state = BCC1_RCV;
				else {
					if(*c == FLAG)
						*state = FLAG_RCV;
					else
						*state = START;
				}
				break;

			case(BCC1_RCV):
				if(*c == FLAG)
					*state = DONE;
				else
					*state = START;	

				break;
		}
	}
}


void llclose(int fd) {

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
  close(fd);
}
