#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h>
#include <ctype.h>



#define TRUE                1
#define FALSE               0

#define SERVER_PORT         21
#define SERVER_ADDR         "192.168.28.96"

#define MAX_SIZE            100
#define SOCKET_ARR_SIZE     1500

enum state_machine_1 {START, SPACE, MINUS, DONE};
enum state_machine_2 {START2, SPACE2, OBRACKET, PORT1, PORT2, DONE2};

typedef struct {
    char username[MAX_SIZE];
    int usernameLength;

    char password[MAX_SIZE];
    int passwordLength;

    char server[MAX_SIZE];
    int serverLength;

    char path[MAX_SIZE];
    int pathLength;

    char filename[MAX_SIZE];
    int filenameSize;

    char response[3];

    int newPort;

} Info;



int parseArgument(char *argument, Info *info);
void getFilename(Info *info);
struct hostent *retrieveIP(Info *info);
int getAnswer(int sockfd, Info *info);
int sendUsername(int sockfd, Info *info);
int sendPassword(int sockfd, Info *info);
void getPort(int sockfd, Info *info);
int sendRetrieveCommand(int sockfd, int sockfdClient, Info *info);
void makeFile(int fd, Info *info);

// testar com -> ftp://anonymous:1@speedtest.tele2.net/1KB.zip
int main(int argc, char **argv) {

    int sockfd;
    int sockfdClient;
    struct hostent *h;
    struct sockaddr_in server_addr;
    struct sockaddr_in server_addr_client;

    if(argc < 2) {
        printf("Invalid number of arguments! \n");
        exit(0);
    }


    Info info;
    memset(info.username, 0, MAX_SIZE);
    memset(info.password, 0, MAX_SIZE);
    memset(info.server, 0, MAX_SIZE);
    memset(info.path, 0, MAX_SIZE);
    memset(info.filename, 0, MAX_SIZE);
    memset(info.response, 0, 3);


    if( parseArgument(argv[1], &info) == -1 ) {
        printf("Invalid argument! \n");
        exit(0);
    }

    getFilename(&info);

	printf("%s \n", info.username);
	printf("%s \n", info.password);
	printf("%s \n", info.server);
	printf("%s \n", info.path);
	printf("%s \n", info.filename);
	printf("%s \n", info.response);

    h = retrieveIP(&info);


    /*server address handling*/
    bzero((char*)&server_addr,sizeof(server_addr));

	server_addr.sin_family = AF_INET;

	server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr)));	/*32 bit Internet address network byte ordered*/

	server_addr.sin_port = htons(SERVER_PORT);		/*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	if( (sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0 ) {
        perror("socket()");
        exit(0);
    }

	/*connect to the server*/
    if( connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ) {
        perror("connect()");
        exit(0);
    }

	
    getAnswer(sockfd, &info);

    if( info.response[0] == '1') {
        getAnswer(sockfd, &info);
    }
    else if( info.response[0] == '2') {
        printf(" > Connection made. \n");
    }
    else if( info.response[0] == '4' ) {
        while( info.response[0] == '4' ) {
            if( connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ) {
                perror("connect()");
                close(sockfd);
                exit(0);
            }

            getAnswer(sockfd, &info);
        }
    }
    else if( info.response[0] == '5' ) {
        printf(" > An error was met trying to connect.\n");
        return -1;
    }

    printf(" > Logging in. \n");

    int ans;
	
	ans = sendUsername(sockfd, &info);
	
	if(ans == 2)
		ans = sendPassword(sockfd, &info);
	else if( ans == -1) {
		return -1;
	}

	if( ans == -1 )
		return -1;

    getPort(sockfd, &info);
	printf("\n");

    /*server address handling*/
	bzero((char *)&server_addr_client, sizeof(server_addr_client));
	server_addr_client.sin_family = AF_INET;
	server_addr_client.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr))); /*32 bit Internet address network byte ordered*/
	server_addr_client.sin_port = htons(info.newPort);										   /*server TCP port must be network byte ordered */

	/*open an TCP socket*/
	if ((sockfdClient = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket()");
		exit(0);
	}

	/*connect to the server*/
	if (connect(sockfdClient, (struct sockaddr *)&server_addr_client, sizeof(server_addr_client)) < 0)
	{
		perror("connect()");
		exit(0);
	}
	


    ans = sendRetrieveCommand(sockfd, sockfdClient, &info);

	if( ans ){
		close(sockfd);
		close(sockfdClient);
		return 1;
	}
	else 
        printf(" > An error was met trying to download.\n");

	close(sockfd);
	close(sockfdClient);


	return 1;
}

int parseArgument(char *argument, Info *info) {

    char firstPart[] = "ftp://";
    char *garbage;

    int i, k, l;

    for(i=0; i<strlen(firstPart); i++) {
        if( argument[i] != firstPart[i] )
            return -1;
    }


    if( (garbage = strchr(argument + i, ':')) == NULL )
        return -1;

    for(k=0; argument[i] != ':'; i++, k++) {
        info->username[k] = argument[i];
    }
    info->usernameLength = k;
    i++;


    if( (garbage = strchr(garbage, '@')) == NULL )
        return -1;

    for(k=0; argument[i] != '@'; i++, k++) {
        info->password[k] = argument[i];
    }
    info->passwordLength = k;
    i++;


    if( (garbage = strchr(garbage, '/')) == NULL )
        return -1;

    for(k=0; argument[i] != '/'; i++, k++) {
        info->server[k] = argument[i];
    }
    info->serverLength = k;
    i++;


    for(k=0; argument[i] != '\0'; i++, k++) {
        info->path[k] = argument[i];
    }
    info->pathLength = k;
    i++;

}

void getFilename(Info *info) {
    int i, j, k;
	char *garbage;
    
	if( (garbage = strchr(info->path, '/')) == NULL ) {
        	strcpy(info->filename, info->path);
		return;
	}

    for(i=info->pathLength; i>=0; i--) {
        if(info->path[i] == '/')
            break;
    }
	i++;

    for(k = 0, j = i; j<info->pathLength; j++, k++) {
        info->filename[k] = info->path[j];
    }

    info->filenameSize = k;

}

struct hostent *retrieveIP(Info *info) {
    struct hostent *h;

    if( ( (h=gethostbyname(info->server)) == NULL) ) {  
        herror("gethostbyname");
        exit(0);
    }

    return h;
}

int getAnswer(int sockfd, Info *info) {

	memset(info->response, 0, 3);

    enum state_machine_1 state = START;
    char c;

    int counter = 0;
    int multipleLineCounter = 0;

	

    while(state != DONE) {

        read(sockfd, &c, 1);

	printf("%c", c);
        switch(state) {

            case(START):
                if( isdigit(c) ) {
                    info->response[counter] = c;
                    counter++;
                }

                else {
                    if( c == ' ' ) {
                        if( counter == 3 )
                            state = SPACE;
                    }

                    else if( c == '-' ) {
                        if( counter == 3 )
                            state = MINUS;
                    }
                }

                break;

            case(SPACE):
                if( c == '\n' )
                    state = DONE;

                break;

            case(MINUS):
                if( isdigit(c) ) {
                    if( multipleLineCounter != 3 ) {
                        if(info->response[multipleLineCounter] == c) {
                            multipleLineCounter++;
                        }
                    }
                }

                else {
                    if( c == ' ' ) {
                        if( multipleLineCounter == 3 )
                            state = SPACE;
                    }
                    else if( c == '-' )
                        multipleLineCounter = 0;
                }

                break;
        }
    }
}

int sendUsername(int sockfd, Info *info) {

    memset(info->response, 0, 3);
/*    
    char command[5 + info->usernameLength];
    memset(command, 0, 5+info->usernameLength);
    strcpy(command, "user ");
    strcat(command, info->username);
    strcat(command, "\n");	
	
    write(sockfd, command, 5+info->usernameLength);
	*/

	write(sockfd, "user ", strlen("user "));
	write(sockfd, info->username, info->usernameLength);
	write(sockfd, "\n", 1);

    while(TRUE) {
        getAnswer(sockfd, info);

        if( info->response[0] == '1') {
            getAnswer(sockfd, info);
        }
        else if( info->response[0] == '2') {
            return 1;
        }
	else if( info->response[0] == '3') {
	    return 2;
        }
        else if( info->response[0] == '4' ) { 
            //write(sockfd, command, 5+info->usernameLength);
		write(sockfd, "user ", strlen("user "));
		write(sockfd, info->username, info->usernameLength);
		write(sockfd, "\n", 1);
        }
        else if( info->response[0] == '5' ) {
            printf("> An error was met trying to login.\n");
            return -1;
        }

	
    }
}

int sendPassword(int sockfd, Info *info) {

    memset(info->response, 0, 3);
   /* 
    char command[5 + info->passwordLength];
    memset(command, 0, 5+info->passwordLength);
    strcpy(command, "pass ");
    strcat(command, info->password);
    strcat(command, "\n");
    write(sockfd, command, 5+info->passwordLength);
*/
	write(sockfd, "pass ", strlen("pass "));
	write(sockfd, info->password, info->passwordLength);
	write(sockfd, "\n", 1);

    while(TRUE) {
        getAnswer(sockfd, info);

         if( info->response[0] == '1') {
            getAnswer(sockfd, info);
        }
        else if( info->response[0] == '2') {
            return 1;
        }
        else if( info->response[0] == '4' ) { 
            //write(sockfd, command, 5+info->passwordLength);
	write(sockfd, "pass ", strlen("pass "));
	write(sockfd, info->password, info->passwordLength);
	write(sockfd, "\n", 1);
        }
        else if( info->response[0] == '5' ) {
            printf("> An error was met trying to login.\n");
            return -1;
        }
    }
}

void getPort(int sockfd, Info *info) {

    enum state_machine_2 state = START2;
    char c;
    int counter = 0;
    int nBytes = 0;
    unsigned char byte1[4];
    int byte1Size = 0;

    unsigned char byte2[4];
    int byte2Size = 0;

    memset(byte1, 0, 4);
    memset(byte2, 0, 4);

    write(sockfd, "pasv\n", 5);


	while(state != DONE2) {
	read(sockfd, &c, 1);
	printf("%c", c);
        switch(state) {

            case(START2):
                if( isdigit(c) ) {
                    info->response[counter] = c;
                    counter++;
                }

                else {
                    if( c == ' ' ) {
                        if( counter == 3 )
                            state = SPACE;
                    }
                }

                break;

            case(SPACE2):
                if( c == '(' )
                    state = OBRACKET;

                break;

            case(OBRACKET):
                if( c == ',' ) {
			nBytes++;
                    	if( nBytes == 4 )
                        	state=PORT1;
			
                }
		

                break;

            case(PORT1):
                if( c != ',' ) {

                    byte1[byte1Size] = c;
                    byte1Size++;
                }
                else {
                    state = PORT2;
		}
                break;

            case(PORT2):
                if( c != ')' ) {
			if( c != ',' ) {

                    		byte2[byte2Size] = c;
                    		byte2Size++;
			}
                }
                else {
                    state = DONE2;
		}

                break;
        }
    }

    info->newPort = atoi(byte1) * 256 + atoi(byte2);
}

int sendRetrieveCommand(int sockfd, int sockfdClient, Info *info) {
    memset(info->response, 0, 3);
   /* 
    char command[5 + info->pathLength];
    memset(command, 0, 5+info->pathLength);
    strcpy(command, "retr ");
    strcat(command, info->path);
    strcat(command, "\n");
    write(sockfd, command, 5+info->pathLength);
*/
	write(sockfd, "retr ", strlen("retr "));
	write(sockfd, info->path, info->pathLength);
	write(sockfd, "\n", 1);

    while(TRUE) {
        getAnswer(sockfd, info);

        if( info->response[0] == '1') {
            makeFile(sockfdClient, info);
        }
        else if( info->response[0] == '2') {
            return 1;
        }
        else if( info->response[0] == '4' ) { 
            //write(sockfd, command, 5+info->passwordLength);
		write(sockfd, "retr ", strlen("retr "));
		write(sockfd, info->path, info->pathLength);
		write(sockfd, "\n", 1);
        }
        else if( info->response[0] == '5' ) {
            printf("> An error was met trying to login.\n");
            return -1;
        }
    }
}

void makeFile(int fd, Info *info) {
	FILE *file = fopen((char *)info->path, "wb+");

	char socketArr[SOCKET_ARR_SIZE];
 	int bytes;

	
 	while( (bytes = read(fd, socketArr, SOCKET_ARR_SIZE)) > 0 ) {

    		fwrite(socketArr, sizeof(int), bytes, file);
   	 }

  	fclose(file);

	printf(" > File download finish. \n");
}
