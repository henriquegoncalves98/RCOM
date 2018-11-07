#include "application.h"

ApplicationLayer *applicationLayer;

void startApplication(unsigned char* port, int mode, int maxBytesPerPacket, int retries, int timeout, unsigned char *fileName) {

    (*applicationLayer).status = mode;
    (*applicationLayer).fileName = fileName;

    strncpy((*linkLayer).port, port, strlen(port));
    (*linkLayer).bytesForEachPacket = maxBytesPerPacket;
    (*linkLayer).NUM_RETRIES = retries;
    (*linkLayer).timeout = timeout;

    (*applicationLayer).fd = open((*linkLayer).port, O_RDWR | O_NOCTTY );
	  if( (*applicationLayer).fd < 0 ) {
		     perror((*linkLayer).port);
         exit(-1);
	  }

    if( (*applicationLayer).status )
      startTransmiterApp();
    else
      startReceiverApp();
}


void startTransmiterApp() {

  //prepare alarmhandler

  if( llopen((*applicationLayer).fd, (*applicationLayer).status) == 0 ) {
  		printf("Failed to make a connection to the receiver \n");
  		exit(1);
  	}

  off_t fileSize;
  off_t indice = 0;

	unsigned char *message = readFile((*applicationLayer).fileName, &fileSize);
	int fileNameSize = strlen((*applicationLayer).fileName);

	//getting the value of the file size in a char array to minimize the bytes used from fileSize
	char fileSizeBuf[sizeof(fileSize) + 2];
	snprintf(fileSizeBuf, sizeof fileSizeBuf, "%ld", fileSize);

	//send start frame
	sendControlFrame((*applicationLayer).fd, C_START, fileSizeBuf, (*applicationLayer).fileName);

	int sizeDP = (*linkLayer).bytesForEachPacket;

	/* RESTO DO PROTOCOLO (INFO FRAMES ETC.) */
	 while (indice < fileSize) {

		//cut the message in the bytesForEachPacket
		unsigned char *data_packet = cutMessage(message, &indice, &sizeDP, fileSize);
		printf("Sent packet nr %d\n", (*linkLayer).numTotalPackets);

		//packet header
		int headerSize = sizeDP;
		unsigned char *final_data_packet = packetHeader(data_packet, &headerSize, fileSize);

		//sends the data packet
		if (!llwrite((*applicationLayer).fd, final_data_packet, headerSize)) {
			printf("Error in llwrite\n");
			exit(1);
		}
	}

	//send end frame
	sendControlFrame((*applicationLayer).fd, C_END, fileSizeBuf, (*applicationLayer).fileName);


  sendSUFrame((*applicationLayer).fd, DISC_C);
	printf("DISC frame sent \n");

	caughtSUFrame((*applicationLayer).fd, DISC_C);
	printf("DISC frame caught \n");

	sendSUFrame((*applicationLayer).fd, UA_C);
	printf("UA frame sent \n");

	llclose((*applicationLayer).fd);
}

void startReceiverApp() {

    llopen((*applicationLayer).fd, (*applicationLayer).status);

  unsigned char *startFrame = (unsigned char *)malloc(0);
	int startFrameSize;

	do {
		startFrameSize = llread((*applicationLayer).fd, startFrame);
	}while(startFrameSize == -1);
	printf("START frame received \n");

	Message message;

	getFileInfo(startFrame, startFrameSize, &message);
	unsigned char fileData[message.fileSize];
	int fileDataSize = 0;

	int received2 = FALSE;
	unsigned char packet[(*linkLayer).bytesForEachPacket+5];
	int packetSize = 0;

	while(!received2) {

		do {
			packetSize = llread((*applicationLayer).fd, packet);
		} while(packetSize == -1);

		if( hasFinishedReceiving(packet, packetSize, startFrame, startFrameSize) )
			received2 = TRUE;
		else {
			received2 = FALSE;
			getPacketInfo(fileData, &fileDataSize, packet, packetSize);
		}

	}

	makeNewFile(message, fileData, fileDataSize);

  caughtSUFrame((*applicationLayer).fd, DISC_C);
	printf("DISC frame received \n");

	sendSUFrame((*applicationLayer).fd, DISC_C);
	printf("DISC frame sent \n");

	caughtSUFrame((*applicationLayer).fd, UA_C);
	printf("UA frame received \n");

	llclose((*applicationLayer).fd);
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

int hasBCC2(unsigned char *buffer, int sizebuffer) {
	int i = 1;
	unsigned char BCC2 = buffer[0];
	for (; i < (sizebuffer - 1); i++)
	{
		BCC2 ^= buffer[i];
	}

	if (BCC2 == buffer[sizebuffer - 1])
		return TRUE;
	else
		return FALSE;
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

unsigned char *packetHeader(unsigned char *message, int *sizeDP, off_t fileSize) {
	*sizeDP += 4;
	unsigned char * finalPacket = (unsigned char *)malloc(*sizeDP);
	//packet header
	finalPacket[0] = C_DATA;
	finalPacket[1] = (*linkLayer).numTotalPackets % 255;
	finalPacket[2] = (int)fileSize / 256;
	finalPacket[3] = (int)fileSize % 256;
	//now we need to concatenate the header with the body of the final packet
  memcpy(finalPacket + 4, message, ((*sizeDP)-4));
	(*linkLayer).numTotalPackets++;

	return finalPacket;
}

void getFileInfo(unsigned char *startFrame, int startFrameSize, Message *message) {
	int l1 = (int)startFrame[2];

	int l2 = (int)startFrame[2 + l1 + 2];
	unsigned char *name = (unsigned char*)malloc(l2);
	int i;
	for(i=0; i<l2; i++) {
		name[i] = startFrame[2 + l1 + 3 + i];
	}

	(*message).fileName = (unsigned char*)malloc(l2);
	memcpy((*message).fileName, name, l2);
	(*message).fileNameLength = l2;


	unsigned char *size = (unsigned char *)malloc(l1);

	for(i=0; i<l1; i++) {
		size[i] = startFrame[3 + i];
	}

	(*message).fileSize = atoi((const char *)size);


	free(name);
	free(size);
}

int hasFinishedReceiving(unsigned char *packet, int packetSize, unsigned char *startFrame, int startFrameSize) {
	int i;
	if(packet[0] == C_END) {
		for(i=1; i<packetSize; i++) {
			if( packet[i] != startFrame[i] )
				return FALSE;
		}
		printf("END frame received \n");
		return TRUE;
	}

	return FALSE;
}

void getPacketInfo(unsigned char *fileData, int *fileDataSize, unsigned char *packet, int packetSize) {

	unsigned char *fileData2 = (unsigned char *)malloc(packetSize - 4);
	int i, j;
	for(i=4, j=0; i<packetSize; i++, j++) {
		fileData2[j] = packet[i];
	}

	for( i=0; i<(packetSize - 4); i++) {
		fileData[(*fileDataSize) + i] = fileData2[i];
	}

	(*fileDataSize) += (packetSize - 4);

	free(fileData2);
}

void makeNewFile(Message message, unsigned char *fileData, int fileDataSize) {
	FILE *f;

	f = fopen((char *)message.fileName, "wb+");

	fwrite((void *)fileData, sizeof(unsigned char), fileDataSize, f);

	fclose(f);
}
