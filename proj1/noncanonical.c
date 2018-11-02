/*Non-Canonical Input Processing*/
#include "noncanonical.h"

#define BAUDRATE 			B38400
#define _POSIX_SOURCE 		1 /* POSIX compliant source */
#define FALSE 				0
#define TRUE 				1
			

volatile int STOP=FALSE;

struct termios oldtio,newtio;


int main(int argc, char** argv) {
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

	unsigned char *startFrame;

	startFrame = llread(fd);

	// TODO processar a recepcao da mensagem


	// TODO processar a recepcao do endFrame

	llclose(fd);
	return 0;
}

void llopen(int fd) {
	
	if ( tcgetattr(fd,&oldtio) == -1) {  //save current port settings 
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


	if( caughtSUFrame(fd, SET_C) ) {
		printf("SET frame received \n");
		sendAcknowlegment(fd, UA_C);
		printf("UA frame sent \n");
	}
}

int caughtSUFrame(int fd, unsigned char CFlag) {

	unsigned char c;
	enum state_machine state = START;

	while(state != DONE) {

		read(fd, &c, 1);

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
				if(c == CFlag)
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
				if(c == FLAG)
					state = DONE;
				else
					state = START;	

				break;
		}
	}

	return TRUE;
}


unsigned char *llread(int fd) {
	
	// TODO finalizar llread

	unsigned char c;
	enum state_machine state = START;
	unsigned char C_Flag;
	int frameNumber = 0;

	while(state != DONE) {

		read(fd, &c, 1);

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
				if(c == C0) {
					state = BCC1_RCV;
					C_Flag = c;
					frameNumber = 0;
				}
				else if(c == C1) {
					state = BCC1_RCV;
					C_Flag = c;
					frameNumber = 1;
				}
				else {
					if(c == FLAG)
						state = FLAG_RCV;
					else
						state = START;
				}
				break;
			
			case(C_RCV):
				if( c == (A ^ C_Flag) )
					state = BCC1_RCV;
				else if(c == FLAG) {
					state = FLAG_RCV;
				}
				else {
					state = START;
				}
				break;

			case(BCC1_RCV):
				if(c == FLAG) {
					if( hasBCC2() ) {
						if( frameNumber == 0)
							sendAcknowlegment(fd, RR_C1);
						else
							sendAcknowlegment(fd, RR_C0);
						
						state = DONE;
					}
					else {
						if( frameNumber == 0)
							sendAcknowlegment(fd, REJ_C1);
						else
							sendAcknowlegment(fd, REJ_C0);

						state = DONE;
					}
				}
				break;
		}
	}
}

int hasBCC2() {

	// TODO finalizaar has BCC2
	
	int var = TRUE;

	if(var)
		return TRUE;
	else
		return FALSE;
}

void sendAcknowlegment(int fd, unsigned char c) {
	unsigned char ackFrame[5];

	ackFrame[0] = FLAG;
	ackFrame[1] = A;
	ackFrame[2] = c;
	ackFrame[3] = ackFrame[1]^ackFrame[2];
	ackFrame[4] = FLAG;

	write(fd, ackFrame, 5);
}

void llclose(int fd) {
	
	tcsetattr(fd,TCSANOW,&oldtio);
	

	caughtSUFrame(fd, DISC_C);
	printf("DISC frame received \n");

	sendAcknowlegment(fd, DISC_C);
	printf("DISC frame sent \n");

	caughtSUFrame(fd, UA_C);
	printf("DISC frame received \n");

	close(fd);
}
