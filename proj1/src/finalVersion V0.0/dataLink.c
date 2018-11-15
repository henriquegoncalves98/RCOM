#include "dataLink.h"

struct termios oldtio,newtio;

LinkLayer *linkLayer;

int llopen(int fd, int mode) {

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

	if ( tcsetattr(fd, TCSANOW, &newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	printf("New termios structure set\n");

  if( mode ) {

  	sendSUFrame(fd, SET_C);
  	printf("SET frame sent \n");

    //TODO set alarm
    //setAlarm();
  	//alarm((*linkLayer).timeout);

  	enum state_machine state = START;
  	unsigned char c;

  	while( !(*alarm).received && ((*alarm).retries != (*linkLayer).NUM_RETRIES) ) {

  		if( (*alarm).resend ) {
  			sendSUFrame(fd, SET_C);
        /*
        setAlarm();
  			alarm(timeout);
  			resend = FALSE;
  			state = START;
        */
  		}
  		else {
  			read(fd, &c, 1);
  			caughtUA(&state, &c);

  			if(state == DONE)
  			{
  				printf("UA received \n");
  				(*alarm).received = TRUE;
  			}
  		}
  	}

  	if((*alarm).retries == (*linkLayer).NUM_RETRIES) {
  		printf("Number of tries exceeded the maximum value \n");
  		return 0;
  	}
  }
  else {
    if( caughtSUFrame(fd, SET_C) ) {
  		printf("SET frame received \n");
  		sendSUFrame(fd, UA_C);
  		printf("UA frame sent \n");
  	}
  }

	return 1;
}

int llwrite(int fd, unsigned char * buffer, int length) {

	unsigned char BCC2;
  unsigned char *BCC2Stuffed = (unsigned char *)malloc(sizeof(unsigned char));
	unsigned char *Iarray = (unsigned char *)malloc((length + 6) * sizeof(unsigned char));
	int IarraySize = length + 6;
	int sizeBCC2 = 1;
	BCC2 = calculoBCC2(buffer, length);
	BCC2Stuffed = stuffingBCC2(BCC2, &sizeBCC2);

	Iarray[0] = FLAG;
	Iarray[1] = A;

	if((*linkLayer).frameExpected == 0) {
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
		//retries = 0;
	//	resend = FALSE;
		//(*alarm).received = FALSE;

		bytesW = write(fd, Iarray, IarraySize);

		//ativa alarme
		//alarm(timeout);

		//verifica se recebeu um acknolagement
		enum state_machine state = START;
		unsigned char c;
		unsigned char ctrl;


		while( !(*alarm).received && ((*alarm).retries != (*linkLayer).NUM_RETRIES) ) {

			if( (*alarm).resend ) {
				bytesW = write(fd, Iarray, IarraySize);
				//alarm(timeout);
				(*alarm).resend = FALSE;
				state = START;
			}
			else {
				read(fd, &c, 1);
				checkACK(&state, &c, &ctrl);

				if(state == DONE) {
					printf("ACK/NACK received \n");
					(*alarm).received = TRUE;
				}
			}
		}

		if( (ctrl == RR1_C && (*linkLayer).frameExpected == 0) || (ctrl == RR0_C && (*linkLayer).frameExpected == 1) )
		{
			if(ctrl == RR1_C)
				printf("RECEIVED RR1\n");
			else
				printf("RECEIVED RR0\n");

			repeat_frame = FALSE;
		//	retries = 0;
			(*linkLayer).frameExpected ^= 1;
			//alarm(0);
		}
		else if( ctrl == REJ0_C || ctrl == REJ1_C )
		{
			if(ctrl == REJ1_C)
				printf("RECEIVED REJ1\n");
			else
				printf("RECEIVED REJ0\n");

			repeat_frame = TRUE;
			//retries = 0;
			//alarm(0);
		}

	} while(repeat_frame);

  free(Iarray);

	if ((*alarm).retries >= (*linkLayer).NUM_RETRIES || bytesW <= 0)
		return FALSE;
	else
		return bytesW;
}


int llread(int fd, unsigned char * buffer) {

	//TODO Quando BCC2 errado, Se se tratar dum duplicado, deve fazer-se confirmação com RR
	unsigned char* otherBuff = (unsigned char*)malloc(0);
	int otherBuffSize = 0;
	unsigned char c;
	enum state_machine state = START;
	unsigned char C_Flag;
	int frameNumber = 0;

	int frame_repeat = FALSE;


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
					state = C_RCV;
					C_Flag = c;
					frameNumber = 0;
				}
				else if(c == C1) {
					state = C_RCV;
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
					if( hasBCC2(otherBuff, otherBuffSize) ) {
						if( frameNumber == 0)
							sendSUFrame(fd, RR1_C);
						else
							sendSUFrame(fd, RR0_C);

						state = DONE;
					}
					else {
						if( (frameNumber == 0) && ((*linkLayer).frameExpected == 1) )
							sendSUFrame(fd, RR1_C);
						else if( (frameNumber == 1) && ((*linkLayer).frameExpected == 0) )
							sendSUFrame(fd, RR0_C);
						else {
							if( frameNumber == 0)
								sendSUFrame(fd, REJ0_C);
							else
								sendSUFrame(fd, REJ1_C);
						}

						state = DONE;
						frame_repeat = TRUE;
					}
				}
				else if(c == ESCAPE)
					state = ESCAPING;

				else {
					otherBuff = (unsigned char *)realloc(otherBuff, ++otherBuffSize);
       				otherBuff[otherBuffSize - 1] = c;
				}

				break;

			case(ESCAPING):
				if( c == ESCAPE_FLAG )
				{
					otherBuff = (unsigned char *)realloc(otherBuff, ++otherBuffSize);
					otherBuff[otherBuffSize - 1] = FLAG;
				}
				else if(c == ESCAPE_ESCAPE){

					otherBuff = (unsigned char *)realloc(otherBuff, ++otherBuffSize);
       				otherBuff[otherBuffSize - 1] = ESCAPE;
				}
				else{

					perror("Non valid character after escape character");
          			exit(-1);
				}
				state = BCC1_RCV;
				break;
		}
	}

	if(frame_repeat)
		return -1;



	printf("Information packet received with size: %d\n", otherBuffSize);
	//Removes BCC2 from end of frame
	otherBuff = (unsigned char *)realloc(otherBuff, --otherBuffSize);

	if (otherBuffSize > 0)
	{
		if (frameNumber == (*linkLayer).frameExpected)
			(*linkLayer).frameExpected ^= 1;
		else
			otherBuffSize = -1;
	}

	int i;
	for(i=0; i<otherBuffSize; i++) {
		buffer[i] = otherBuff[i];
	}


	return otherBuffSize;
}

void llclose(int fd) {

  if((*applicationLayer).status) {
  	if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
  		perror("tcsetattr");
  		exit(-1);
  	}

  	close(fd);
  }
  else {
    tcsetattr(fd,TCSANOW,&oldtio);

    close(fd);
  }
}
