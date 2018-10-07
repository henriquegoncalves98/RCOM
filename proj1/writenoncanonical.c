/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

struct termios oldtio,newtio;


int main(int argc, char** argv)
{
  int fd, c, res;
  char buf[255];
  int i, sum = 0, speed = 0;
  
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


	llopen(fd);



  

	char test[255];
	test[0] = 0x7E;
	test[1] = 0x03;
	test[2] = 0x03;
	test[3] = test[1]^test[2];
	test[4] = test[0];

	

	int j;
	for(j=0;j<5;j++)
		printf("[%x] ",test[j]);
	printf("\n");
	
	res = write(fd,test,strlen(test)); 

	/*
	printf("Introduza uma string :");
	fgets(buf,254,stdin);

	printf("%s", buf);  
    
    res = write(fd,buf,strlen(buf)+1);   
    printf("%d bytes written\n", res);
 */

  /* 
    O ciclo FOR e as instru��es seguintes devem ser alterados de modo a respeitar 
    o indicado no gui�o 
  */
/*
	while (STOP == FALSE) {
		res = read(fd, buf, 255);
		printf(":%s:%d\n", buf, res);
		if (buf[res-1]=='\0') STOP=TRUE;
	}
	*/


  llclose(fd);

  close(fd);
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
  newtio.c_cc[VMIN]     = 1;   /* blocking read until 1 char received */
/* 
  VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
  leitura do(s) pr�ximo(s) caracter(es)
*/



  tcflush(fd, TCIOFLUSH);

  if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");
}


void llclose(int fd) {

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
}
