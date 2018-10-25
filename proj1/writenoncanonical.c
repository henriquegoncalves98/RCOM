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
#define C0					0x00
#define C1					0x40
#define RR0_C				0x05
#define RR1_C				0x85
#define REJ0_C				0x01
#define REJ1_C				0x81

#define ESCAPE			    0x7D
#define ESCAPE_FLAG         0x5E
#define ESCAPE_ESCAPE       0x5D


volatile int STOP=FALSE;

struct termios oldtio, newtio;
int timeout = 3;
int retries = 0;
int resend = FALSE;
int received = FALSE;
int messageToSend = 0;
int acknowladgement = 0;


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
	
	//cria mensagem a enviar
	char message1[2];
	message1[0] = 0x34;
	message1[1] = 0x35;

	char message2[2];
	message2[0] = 0x36;
	message2[1] = 0x37;
	
	//envia mensagem
	llwrite(fd, message1, 2);

	//ISTO VAI PARA O LLWRITE!!!!!!!!!!!!!!!!!!!!!!!!apagar depois
	//ativa alarme
	alarm(timeout);

	//verifica se recebeu um acknolagement
	enum state_machine state = START;
	unsigned char c;

	//reset time out parameters
	retries = 0;
	resend = FALSE;
	received = FALSE;
	
	while( !received && (retries != 3) ) {

		if( resend ) { 
			if(messageToSend == 0) {
				llwrite(fd, message1, 2);
				alarm(timeout);
				resend = FALSE;
				state = START;
			}
			else {
				llwrite(fd, message2, 2);
				alarm(timeout);
				resend = FALSE;
				state = START;
			}
		}
		else {
			read(fd, &c, 1);
			caughtACK(&state, &c);

			if(state == DONE)
			{
				printf("UA received \n");
				received = TRUE;
			}
		}
	}
	//acknolagement == ACK, envia a proxima mensagem
	//acknolagement == NACK, envia a mesma mensagem
	//caso alarme acione. Manda a mensagem actual



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
	
	while( !received && (retries != 3) ) {

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

	if(retries == 3) {
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
  	BCC2 = calculoBCC2(mensagem, size);
	BCC2Stuffed = stuffingBCC2(BCC2, &sizeBCC2);

	Iarray[0] = FLAG;
	Iarray[1] = A;
	
	if(messageToSend == 0)
	{
		Iarray[2] = C0;
	}
	else {
		Iarray[2] = C1;
	}

	Iarray[3] = SET_BCC1;

	int i = 0;
	int j = 4;
	//stuffing and attributing data to frame I
	for(; i < length;i++)
	{
		if(buffer[i] == FLAG)
		{
			Iarray = (unsigned char *)realloc(Iarray, ++IarraySize);
			Iarray[j] = ESCAPE;
			Iarray[j + 1] = ESCAPE_FLAG;
			j += 2;
		} 
		else if(buffer[i] == ESCAPE)
		{
			Iarray = (unsigned char *)realloc(Iarray, ++IarraySize);
			Iarray[j] = ESCAPE;
			Iarray[j + 1] = ESCAPE_ESCAPE;
			j += 2;
		}
		else
		{
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

	//send message now

	write(fd, Iarray, IarraySize);

	//ativa alarme
	alarm(timeout);

	//verifica se recebeu um acknolagement
	enum state_machine state = START;
	unsigned char c;
	unsigned char ctrl;

	//reset time out parameters
	retries = 0;
	resend = FALSE;
	received = FALSE;

	while( !received && (retries != 3) ) {

		if( resend ) { 
			write(fd, Iarray, IarraySize);
			alarm(timeout);
			resend = FALSE;
			state = START;
		}
		else {
			read(fd, &c, 1);
			caughtACK(&state, &c, &ctrl);

			if(state == DONE)
			{
				printf("ACK/NACK received \n");
				received = TRUE;
			}
		}
	}
	//acknolagement == ACK, envia a proxima mensagem
	//acknolagement == NACK, envia a mesma mensagem
	//caso alarme acione. Manda a mensagem actual

	//falta processar o CTRL!!!!!!!!

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




void llclose(int fd) {

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
  close(fd);
}
