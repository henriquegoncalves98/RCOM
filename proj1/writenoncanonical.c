/*Non-Canonical Input Processing*/
#include "writenoncanonical.h"

#define BAUDRATE 			B38400
#define MODEMDEVICE 		"/dev/ttyS1"
#define _POSIX_SOURCE 		1 /* POSIX compliant source */
#define FALSE 				0
#define TRUE 				1


volatile int STOP=FALSE;

struct termios oldtio, newtio;
int timeout = 3;
int retries = 0;
int NUM_RETRIES = 3;
int resend = FALSE;
int received = FALSE;
int messageToSend = 0;


static void alarmHandler(int sig)
{
  printf("Failed to receive frame! \n");
  retries++;
  resend = TRUE;
  received = FALSE;
}


int main(int argc, char** argv)
{
	int fd;
	off_t fileSize;

	
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
	
	// TODO mudar nome do ficheiro para argumento do main
	unsigned char *message = readFile("pinguim.gif", &fileSize);

	int fileNameSize = strlen("pinguim.gif");
	unsigned char *fileName = (unsigned char *)malloc(fileNameSize);
	fileName = "pinguim.gif";

	// TODO acabar makeStartFrame
	unsigned char *start = makeStartFrame(fileSize, fileName, fileNameSize);
	
	//envia mensagem
	llwrite(fd, start, 2);

	// TODO tratar do envio da mensagem apos a frame Start
	/* RESTO DO PROTOCOLO (INFO FRAMES ETC.) */

	// TODO tratar do envio da frame end
	llclose(fd);
	return 0;
}



int llopen(int fd)
{
	
	if ( tcgetattr(fd,&oldtio) == -1) { // save current port settings 
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

	alarm(timeout);


	enum state_machine state = START;
	unsigned char c;
	
	while( !received && (retries != NUM_RETRIES) ) {

		if( resend ) { 
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
				printf("UA received \n");
				received = TRUE;
			}
		}
	}

	if(retries == NUM_RETRIES) {
		printf("Number of tries exceeded the maximum value \n");
		return 0;
	}

	return 1;
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

void caughtUA(enum state_machine *state, unsigned char *c) {

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

void checkACK(enum state_machine *state, unsigned char *c, unsigned char *ctrl) {

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
					*state = FLAG_RCV;
				else
					*state = START;
			}
			break;

		case(A_RCV):
			if(*c == RR0_C || *c == RR1_C || *c == REJ0_C || *c == REJ1_C)
			{
				*state = C_RCV;
				*ctrl = *c;
			}
			else {
				if(*c == FLAG)
					*state = FLAG_RCV;
				else
					*state = START;
			}
			break;
	
		case(C_RCV):
			if(*c == (A ^ *ctrl))
				*state = BCC1_RCV;
			else {
				*state = START;
			}
			break;

		case(BCC1_RCV):
			if(*c == FLAG)
			{
				*state = DONE;
			}
			else
				*state = START;	

			break;
	}
}

int llwrite(int fd, char * buffer, int length) {

	unsigned char BCC2;
  	unsigned char *BCC2Stuffed = (unsigned char *)malloc(sizeof(unsigned char));
	unsigned char *Iarray = (unsigned char *)malloc((length + 6) * sizeof(unsigned char));

	int IarraySize = length + 6;
  	int sizeBCC2 = 1;
  	BCC2 = calculoBCC2(buffer, length);
	BCC2Stuffed = stuffingBCC2(BCC2, &sizeBCC2);

	Iarray[0] = FLAG;
	Iarray[1] = A;
	
	if(messageToSend == 0) {
		Iarray[2] = C0;
	}
	else {
		Iarray[2] = C1;
	}

	Iarray[3] = Iarray[1] ^ Iarray[2];

	int i = 0;
	int j = 4;

	//stuffing and attributing data to frame I
	for(; i < length;i++) {

		if(buffer[i] == FLAG) {
			Iarray = (unsigned char *)realloc(Iarray, ++IarraySize);
			Iarray[j] = ESCAPE;
			Iarray[j + 1] = ESCAPE_FLAG;
			j += 2;
		} 
		else if(buffer[i] == ESCAPE) {
			Iarray = (unsigned char *)realloc(Iarray, ++IarraySize);
			Iarray[j] = ESCAPE;
			Iarray[j + 1] = ESCAPE_ESCAPE;
			j += 2;
		}
		else {
			Iarray[j] = buffer[i];
			j++;
		}
	}

	if(sizeBCC2 == 1)
	{
		Iarray[j] = BCC2;
		j++;
	}
	else
	{
		Iarray = (unsigned char *)realloc(Iarray, ++IarraySize);
		Iarray[j] = BCC2Stuffed[0];
		Iarray[j+1] = BCC2Stuffed[1];
		j += 2;
	}

	Iarray[j] = FLAG;

	//reset time out parameters
	retries = 0;
	resend = FALSE;
	received = FALSE;

	int repeat_frame = TRUE;

	//send I message now
	do
	{
		write(fd, Iarray, IarraySize);

		//ativa alarme
		alarm(timeout);

		//verifica se recebeu um acknolagement
		enum state_machine state = START;
		unsigned char c;
		unsigned char ctrl;


		while( !received && (retries != NUM_RETRIES) ) {

			if( resend ) { 
				write(fd, Iarray, IarraySize);
				alarm(timeout);
				resend = FALSE;
				state = START;
			}
			else {
				read(fd, &c, 1);
				checkACK(&state, &c, &ctrl);

				if(state == DONE) {
					printf("ACK/NACK received \n");
					received = TRUE;
				}
			}
		}

		if( (ctrl == RR1_C && messageToSend == 0) || (ctrl == RR0_C && messageToSend == 1) )
		{
			if(ctrl == RR1_C)
				printf("RECEIVED RR1\n");
			else 
				printf("RECEIVED RR0\n");

			repeat_frame = FALSE;
			retries = 0;
			messageToSend ^= 1;
			alarm(0);
		}
		else if( ctrl == REJ0_C || ctrl == REJ1_C )
		{
			if(ctrl == REJ1_C)
				printf("RECEIVED REJ1\n");
			else 
				printf("RECEIVED REJ0\n");

			repeat_frame = TRUE;
			retries = 0;
			alarm(0);
		
		}

	} while(repeat_frame);


	if (retries >= NUM_RETRIES)
		return FALSE;
	else
		return TRUE;
}


unsigned char calculoBCC2(unsigned char *mensagem, int size)
{
  unsigned char BCC2 = mensagem[0];
  int i;
  for (i = 1; i < size; i++)
  {
    BCC2 ^= mensagem[i];
  }
  return BCC2;
}


unsigned char *stuffingBCC2(unsigned char BCC2, int *sizeBCC2)
{
  unsigned char *BCC2Stuffed;
  if (BCC2 == FLAG)
  {
    BCC2Stuffed = (unsigned char *)malloc(2 * sizeof(unsigned char));
    BCC2Stuffed[0] = ESCAPE;
    BCC2Stuffed[1] = ESCAPE_FLAG;
    (*sizeBCC2)++;
  }
  else
  {
    if (BCC2 == ESCAPE)
    {
      BCC2Stuffed = (unsigned char *)malloc(2 * sizeof(unsigned char *));
      BCC2Stuffed[0] = ESCAPE;
      BCC2Stuffed[1] = ESCAPE_ESCAPE;
      (*sizeBCC2)++;
    }
  }

  return BCC2Stuffed;
}

unsigned char *readFile(unsigned char *fileName, off_t *fileSize) {
	
	FILE *f;
	struct stat file;
	unsigned char *fileData;

	fopen(fileName, "rb");

	stat((char *) fileName, &file);

	(*fileSize) = file.st_size;

	fileData = (unsigned char *)malloc(*fileSize);

	fread(fileData, sizeof(unsigned char), *fileSize, f);

	return fileData;
}

unsigned char *makeStartFrame(off_t fileSize, unsigned char *fileName, int fileNameSize) {

	int startFrameSize = 9 * sizeof(unsigned char) + fileNameSize;

	unsigned char *startFrame = (unsigned char *)malloc(startFrameSize);

	startFrame[0] = C_START;
	startFrame[1] = T1;
	
}


void llclose(int fd) {
	
	if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}
	
	// TODO enviar frames finais de confirmacao

	close(fd);
}
