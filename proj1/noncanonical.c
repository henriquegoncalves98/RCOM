/*Non-Canonical Input Processing*/
#include "noncanonical.h"

#define BAUDRATE 			B38400
#define _POSIX_SOURCE 		1 /* POSIX compliant source */
#define FALSE 				0
#define TRUE 				1

#define FLAG 				0x7E
#define A 					0x03
#define SET_C 				0x03
#define UA_C				0x07
#define SET_BCC1 			A^SET_C
#define UA_BCC1 			A^UA_C
			

volatile int STOP=FALSE;

struct termios oldtio,newtio;


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

	llopen(fd);

	/* RESTO DO PROTOCOLO (INFO FRAMES ETC.) */

	llclose(fd);
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
	newtio.c_cc[VMIN]     = 0;   /* blocking read until 5 chars received */

	tcflush(fd, TCIOFLUSH);

	if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	printf("New termios structure set\n");


	if( caughtSET(fd) ) {
		printf("SET frame received");
		//sendUA(fd);
		//printf("UA frame sent");
	}
}


int caughtSET(int fd) {

	char c;
	enum state_machine state = START;

	while(state != DONE) {

		read(fd, c, 1);

		switch(state) {
			case(START):
				if(c == FLAG)
					state = FLAG_RCV;
				break;
			
			case(FLAG_RCV):
				if(c == A)
					state = A_RCV;
				else {
					if(c == FLAG)
						break;
					else
						state = START;
				}
				break;

			case(A_RCV):
				if(c == C_RCV)
					state = C_RCV;
				else {
					if(c == FLAG)
						state = FLAG_RCV;
					else
						state = START;
				}
				break;
			
			case(C_RCV):
				if(c == SET_BCC1)
					state=BCC1_RCV;
				else {
					if(c == FLAG)
						state = FLAG_RCV;
					else
						state=START;
				}
				break;

			case(BCC1_RCV):
				if(c == FLAG_RCV)
					state = DONE;
				else
					state = START;	

				break;
		}
	}

	return TRUE;
}

void sendUA(int fd) {

	char UAarray[5];
	UAarray[0] = FLAG;
	UAarray[1] = A;
	UAarray[2] = UA_C;
	UAarray[3] = UA_BCC1;
	UAarray[4] = FLAG;

	size_t UAarraySize = sizeof(UAarray)/sizeof(UAarray[0]);

	write(fd, UAarray, UAarraySize);

}

void llclose(int fd) {
	tcsetattr(fd,TCSANOW,&oldtio);
	close(fd);
}
