/*Non-Canonical Input Processing*/
#include "writenoncanonical.h"

//#define BAUDRATE 			B38400
#define MODEMDEVICE 		"/dev/ttyS1"
#define _POSIX_SOURCE 		1 /* POSIX compliant source */
#define FALSE 				0
#define TRUE 				1


volatile int STOP=FALSE;

struct termios oldtio, newtio;
int timeout;
int retries = 0;
int NUM_RETRIES;
int resend = FALSE;
int received = FALSE;
int messageToSend = 0;
int bytesForEachPacket;
int numTotalPackets = 0;
int BAUDRATE;


static void alarmHandler(int sig)
{
  printf("Failed to receive frame! \n");
  retries++;
  resend = TRUE;
  received = FALSE;
}

//compiled port NBytesPerPacket MaxNTries timeout baudrate nameOfFile
int main(int argc, char** argv)
{
	int fd;
	off_t fileSize;
	off_t indice = 0;

	  if(argc < 6) {
		printf("Invalid number of arguments! \n");
		exit(1);
	  }
	  else {
		if( (strcmp("/dev/ttyS0", argv[1])!=0) && (strcmp("/dev/ttyS1", argv[1])!=0) ) {
		  printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
		  exit(1);
		}
	  }

	fd = open(argv[1], O_RDWR | O_NOCTTY );
	if (fd <0) {
		perror(argv[1]); exit(-1);
	}
	//END OF PORT CONFIGURATION AND OPENING PORTS

	bytesForEachPacket = atoi(argv[2]);

	NUM_RETRIES = atoi(argv[3]);

	timeout = atoi(argv[4]);

	BAUDRATE = getBAUDRATE(atoi(argv[5]));


	(void) signal(SIGALRM, alarmHandler);

	 struct timespec requestStart, requestEnd;
	 clock_gettime(CLOCK_REALTIME, &requestStart);

	if( llopen(fd) == 0 ) {
		printf("Failed to make a connection to the receiver \n");
		return -1;
	}

	unsigned char *fileName = argv[6];
	unsigned char *message = readFile(fileName, &fileSize);
	int fileNameSize = strlen(fileName);

	//getting the value of the file size in a char array to minimize the bytes used from fileSize
	char fileSizeBuf[sizeof(fileSize) + 2];
	snprintf(fileSizeBuf, sizeof fileSizeBuf, "%ld", fileSize);
	//send start frame
	sendControlFrame(fd, C_START, fileSizeBuf, fileName);

	int sizeDP = bytesForEachPacket;

	/* RESTO DO PROTOCOLO (INFO FRAMES ETC.) */
	srand(time(NULL));
	 while (indice < fileSize)
	{
		//cut the message in the bytesForEachPacket
		printf("\n<---------------------------------------------->\n");
		unsigned char *data_packet = cutMessage(message, &indice, &sizeDP, fileSize);
		printf("Sent packet nr %d\n", numTotalPackets);

		//packet header
		int headerSize = sizeDP;
		unsigned char *final_data_packet = packetHeader(data_packet, &headerSize, fileSize);

		//sends the data packet
		if (!llwrite(fd, final_data_packet, headerSize))
		{
			printf("Error in llwrite\n");
			return -1;
		}
	}

	//T_porp
	clock_gettime(CLOCK_REALTIME, &requestEnd);

	double accum = (requestEnd.tv_sec - requestStart.tv_sec) + (requestEnd.tv_nsec - requestStart.tv_nsec) / 1E9;
	
	printf("\n\n<---------------------------------------------->\n\t\tTOTAL\n");
	printf("Seconds passed: %f seconds\n", accum);
	printf("Total C rate: %f  Bytes/s\n", (double)fileSize / accum);

	printf("<---------------------------------------------->\n");

	//send end frame
	sendControlFrame(fd, C_END, fileSizeBuf, fileName);

	llclose(fd);
	return 0;
}

int getBAUDRATE(int rate) {
	
	if(rate == 38400)
		return B38400;
	else if(rate == 19200)
		return B19200;
	else if(rate == 9600)
		return B9600;
	else if(rate == 4800)
		return B4800;
	else if(rate == 2400)
		return B2400;
	else if(rate == 1800)
		return B2400;
	else if(rate == 1200)
		return B1200;
	else if(rate == 600)
		return B600;
	else if(rate == 300)
		return B300;
	else if(rate == 200)
		return B200;
	else if(rate == 150)
		return B150;
	else if(rate == 134)
		return B134;
	else if(rate == 110)
		return B110;
	else if(rate == 75)
		return B75;
	else if(rate == 50)
		return B50;
	else if(rate == 0)
		return B0;
	else
		return B38400;

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

	newtio.c_cc[VTIME]    = 1;   /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 0;   /* blocking read until 1 char received */


	tcflush(fd, TCIOFLUSH);

	if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}


	printf("New termios structure set\n");

	sendSUFrame(fd, SET_C);
	printf("SET frame sent \n");

	alarm(timeout);


	enum state_machine state = START;
	unsigned char c;

	while( !received && (retries != NUM_RETRIES) ) {

		if( resend ) {
			sendSUFrame(fd, SET_C);
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

void sendSUFrame(int fd, unsigned char c) {

	char frame[5];
	frame[0] = FLAG;
	frame[1] = A;
	frame[2] = c;
	frame[3] = frame[1]^frame[2];
	frame[4] = FLAG;

	size_t frameSize = sizeof(frame)/sizeof(frame[0]);

	write(fd, frame, frameSize);
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

void caughtDISC(enum state_machine *state, unsigned char *c) {


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
			if(*c == DISC_C)
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

int llwrite(int fd, unsigned char * buffer, int length) {

	unsigned char BCC2;
  	unsigned char *BCC2Stuffed = (unsigned char *)malloc(sizeof(unsigned char) * 2);
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



	int repeat_frame = FALSE;
	ssize_t bytesW;

	//send I message now
	do
	{
		//reset time out parameters
		retries = 0;
		resend = FALSE;
		received = FALSE;
		
		unsigned char *newArray;
		newArray = BCC1changer(Iarray, IarraySize); //altera bcc1
		newArray = BCC2changer(newArray, IarraySize);  //altera bcc2
		bytesW = write(fd, newArray, IarraySize);
	
		//bytesW = write(fd, Iarray, IarraySize);

		//ativa alarme
		alarm(timeout);

		//verifica se recebeu um acknolagement
		enum state_machine state = START;
		unsigned char c;
		unsigned char ctrl;


		while( !received && (retries != NUM_RETRIES) ) {

			if( resend ) {
				bytesW = write(fd, Iarray, IarraySize);
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



  	free(Iarray);

	if (retries >= NUM_RETRIES || bytesW <= 0)
		return FALSE;
	else
		return bytesW;
}


unsigned char calculoBCC2(unsigned char *mensagem, int size) {
  unsigned char BCC2 = mensagem[0];
  int i;
  for (i = 1; i < size; i++)
  {
    BCC2 ^= mensagem[i];
  }
  return BCC2;
}


unsigned char *stuffingBCC2(unsigned char BCC2, int *sizeBCC2) {
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
      BCC2Stuffed = (unsigned char *)malloc(2 * sizeof(unsigned char));
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

	f = fopen(fileName, "rb");

    if(f == NULL) {
	  printf("File not found or doesn't exist \n");
	  exit(1);
    }

	stat((char *) fileName, &file);

	(*fileSize) = file.st_size;

	fileData = (unsigned char *)malloc(*fileSize);

	fread(fileData, sizeof(unsigned char), *fileSize, f);

	fclose(f);

	return fileData;
}

void sendControlFrame(int fd, unsigned char state, char* fileSizeBuf, unsigned char *fileName) {


	if (state == C_START)
		printf("Sending START control package.\n");
	else if (state == C_END)
		printf("Sending END control package.\n");
	else
		printf("Sending UNKNOWN control package.\n");


	int controlFrameSize = 5 + strlen(fileSizeBuf) + strlen(fileName);

	unsigned char *controlFrame = (unsigned char *)malloc(controlFrameSize);
	int ind = 0;
	controlFrame[ind++] = state;
	controlFrame[ind++] = T1;
	controlFrame[ind++] = strlen(fileSizeBuf);
  int i;
	for (i = 0; i < strlen(fileSizeBuf); i++)
		controlFrame[ind++] = fileSizeBuf[i];

	controlFrame[ind++] = T2;
	controlFrame[ind++] = strlen(fileName);
	for (i = 0; i < strlen(fileName); i++)
		controlFrame[ind++] = fileName[i];


	//envia mensagem
	if(!llwrite(fd, controlFrame, controlFrameSize))
	{
		printf("Error in llwrite\n");
		exit(-1);
	}


	if (state == C_START)
		printf("START control package sent.\n");
	else if (state == C_END)
		printf("END control package sent.\n");
	else
		printf("UNKNOWN control package sent.\n");
}

unsigned char *packetHeader(unsigned char *message, int *sizeDP, off_t fileSize) {

	*sizeDP += 4;
	unsigned char * finalPacket = (unsigned char *)malloc(*sizeDP);
	//packet header
	finalPacket[0] = C_DATA;
	finalPacket[1] = numTotalPackets % 255;
	finalPacket[2] = (int)fileSize / 256;
	finalPacket[3] = (int)fileSize % 256;
	//now we need to concatenate the header with the body of the final packet
  memcpy(finalPacket + 4, message, ((*sizeDP)-4));
	numTotalPackets++;

	return finalPacket;
}

unsigned char *cutMessage(unsigned char *message, off_t *indice, int *sizeDP, off_t fileSize) {

	if((*indice + *sizeDP) > fileSize)
	{
		*sizeDP = fileSize - *indice;
	}

	unsigned char * packet = (unsigned char *)malloc(*sizeDP);
  int i;
  for(i = 0; i < *sizeDP; i++, (*indice)++)
	{
		packet[i] = message[*indice];
	}

	return packet;
}

unsigned char *BCC2changer(unsigned char *packet, int sizePacket) {

  unsigned char *newArray = (unsigned char *)malloc(sizePacket);
  memcpy(newArray, packet, sizePacket);
  int r = (rand() % 100) + 1;
  if (r <= bcc2ErrorPercentage)
  {
    int i = (rand() % (sizePacket - 5)) + 4;
    unsigned char randomLetter = (unsigned char)('A' + (rand() % 26));
    newArray[i] = randomLetter;
    printf("Modifiquei BCC2\n");
  }
  return newArray;
}

unsigned char *BCC1changer(unsigned char *packet, int sizePacket) {

  unsigned char *newArray = (unsigned char *)malloc(sizePacket);
  memcpy(newArray, packet, sizePacket);
  int r = (rand() % 100) + 1;
  if (r <= bcc1ErrorPercentage)
  {
    int i = (rand() % 3) + 1;
    unsigned char randomLetter = (unsigned char)('A' + (rand() % 26));
    newArray[i] = randomLetter;
    printf("Modifiquei BCC1\n");
  }
  return newArray;
}

void llclose(int fd) {

	sendSUFrame(fd, DISC_C);
	printf("DISC frame sent \n");
	
	resend = TRUE;
	received = FALSE;
	alarm(timeout);

	enum state_machine state = START;
	unsigned char c;

	while( !received && (retries != NUM_RETRIES) ) {

		if( resend ) {
			sendSUFrame(fd, DISC_C);
			alarm(timeout);
			resend = FALSE;
			state = START;
		}
		else {
			read(fd, &c, 1);
			caughtDISC(&state, &c);

			if(state == DONE)
			{
				printf("DISC received \n");
				received = TRUE;
			}
		}
	}

	if(retries == NUM_RETRIES) {
		printf("Number of tries exceeded the maximum value \n");
		exit(-1);
	}

	sendSUFrame(fd, UA_C);
	printf("UA frame sent \n");


	if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	close(fd);
}
