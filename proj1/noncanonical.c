/*Non-Canonical Input Processing*/
#include "noncanonical.h"

#define BAUDRATE 			B38400
#define _POSIX_SOURCE 		1 /* POSIX compliant source */
#define FALSE 				0
#define TRUE 				1


volatile int STOP=FALSE;

int messageToReceive = 0;
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

	llread(fd, startFrame);

	Message message;
	
	getFileInfo(startFrame, message);
	message.fileData = (unsigned char *)malloc(strlen(message.fileSizeBuf));

	int received = FALSE;
 
	unsigned char *packet;

	while(!received) {
		
		llread(fd, packet);

		if( hasFinishedReceiving(packet, startFrame) )
			received = TRUE;
		else {
			received = FALSE;
			
			getPacketInfo(message, packet);
		}
	}

	makeNewFile(message);

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

/*
* Read from file descriptor fd and puts the data read in buffer
* @return negative if failed or the number of characters read, basicly the sizes of buffer if there was no error
*/
int llread(int fd, unsigned char * buffer) {

	//TODO Quando BCC2 errado, Se se tratar dum duplicado, deve fazer-se confirmação com RR
	buffer = (unsigned char *)malloc(0);
	int sizeBuffer = 0;
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
				else {
					state = START;
				}
				break;

			case(BCC1_RCV):
				if(c == FLAG) {
					if( hasBCC2(buffer, sizeBuffer) ) {
						if( frameNumber == 0)
							sendAcknowlegment(fd, RR_C1);
						else
							sendAcknowlegment(fd, RR_C0);

						state = DONE;
					}
					else {
						if( frameNumber == 0)
							sendAcknowlegment(fd, REJ_C0);
						else
							sendAcknowlegment(fd, REJ_C1);

						state = DONE;
						sizeBuffer = -1;
					}
				}
				else if(c == ESCAPE) {

					state = ESCAPING;
				}
				else {
					buffer = (unsigned char *)realloc(buffer, ++sizeBuffer);
       				buffer[sizeBuffer - 1] = c;
				}

				break;

			case(ESCAPING):
				if( c == ESCAPE_FLAG )
				{
						buffer = (unsigned char *)realloc(buffer, ++(sizeBuffer));
						buffer[sizeBuffer - 1] = FLAG;
				}
				else if(c == ESCAPE_ESCAPE){

						buffer = (unsigned char *)realloc(buffer, ++(sizeBuffer));
       			buffer[sizeBuffer - 1] = ESCAPE;
				}
				else{

					perror("Non valid character after escape character");
          			exit(-1);
				}
				state = BCC1_RCV;

				break;
		}
	}

	printf("Frame size: %d\n", sizeBuffer);
	//frame tem BCC2 no fim
	buffer = (unsigned char *)realloc(buffer, --sizeBuffer);

	if (sizeBuffer > 0)
	{
		if (frameNumber == messageToReceive)
		{
			messageToReceive ^= 1;
		}
		else
			sizeBuffer = -1;
	}

	return sizeBuffer;
}

int hasBCC2(unsigned char *buffer, int sizebuffer) {

	int i = 1;
	unsigned char BCC2 = buffer[0];
	for (; i < sizebuffer - 1; i++)
	{
		BCC2 ^= buffer[i];
	}
	if (BCC2 == buffer[sizebuffer - 1])
	{
		return TRUE;
	}
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
/*
      2  3  4  5  6  7  8  9  10 11
 C T1 L1 V1 V2 V3 V4 V5 V6 V7 V8 T2 L2 V1 V2 V3 V4
 A B  8                             4
*/
void getFileInfo(unsigned char *startFrame, Message message) {

	int l1 = (int)startFrame[6];

	int l2 = (int)startFrame[6 + l1 + 2];
	unsigned char *name = (unsigned char*)malloc(l2);

	for(int i=0; i<l2; i++) {
		name[i] = startFrame[6 + l1 + 3 + i];
	}

	message.fileName = name;

	
	char *size = (char *)malloc(l1);

	for(int i=0; i<l1; i++) {
		size[i] = startFrame[7 + i];
	}

	message.fileSizeBuf = size;
}

int hasFinishedReceiving(unsigned char *packet, unsigned char *startFrame) {

	if(packet[4] == C_END) {
		for(int i=4; i<strlen(packet); i++) {
			if( packet[i] != startFrame[i] )
				return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

void getPacketInfo(Message message, unsigned char *packet) {

	unsigned char *fileData = (unsigned char *)malloc(strlen(packet) - 4);

	for(int i=4, j=0; i<(strlen(packet)-4); i++, j++) {
		fileData[j] = packet[i];
	}

	memcpy(message.fileData + strlen(message.fileData), fileData, strlen(fileData));
}

void makeNewFile(Message message) {
	FILE *f;

	f = fopen((char *)message.fileName, "wb+");

	fwrite((void *)message.fileData, message.fileData[0]/sizeof(unsigned char), strlen(message.fileSizeBuf), f);

	fclose(f);
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
