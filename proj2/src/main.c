#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <netdb.h> 
#include <sys/types.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h>

#define SERVER_PORT 6000
#define SERVER_ADDR "192.168.28.96"

#define MAX_SIZE 100

typedef struct {
    char username[MAX_SIZE];
    int usernameLength;

    char password[MAX_SIZE];
    int passwordLength;

    char server[MAX_SIZE];
    int serverLength;

    char path[MAX_SIZE];
    int pathLength;

    char response[3];

} Info;



int parseArgument(char *argument, Info *info);
struct hostent *retrieveIP(Info *info);
void verifyConnection(int sockfd, Info *info);


int main(int argc, char **argv) {

    int sockfd;
    struct hostent *h;
    struct sockaddr_in server_addr;

    if(argc < 2) {
        printf("Invalid number of arguments! \n");
        exit(0);
    }


    Info info;
    memset(info.username, 0, MAX_SIZE);
    memset(info.password, 0, MAX_SIZE);
    memset(info.server, 0, MAX_SIZE);
    memset(info.path, 0, MAX_SIZE);


    if( parseArgument(argv[1], &info) == -1 ) {
        printf("Invalid argument! \n");
        exit(0);
    }

    h = retrieveIP(&info);


    /*server address handling*/
    bzero((char*)&server_addr,sizeof(server_addr));

	server_addr.sin_family = AF_INET;

	server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);	/*32 bit Internet address network byte ordered*/

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

    verifyConnection(sockfd, info);
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

struct hostent *retrieveIP(Info *info) {
    struct hostent *h;

    if( ( (h=gethostbyname(info->server)) == NULL) ) {  
        herror("gethostbyname");
        exit(0);
    }

    return h;
}

void verifyConnection(int sockfd, Info *info) {
    
}