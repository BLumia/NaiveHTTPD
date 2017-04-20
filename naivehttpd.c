// Naive HTTPD
// BLumia (c) 2017
// WTFPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //#include <getopt.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#define PATH_MAX 4096 
#define BUFFER_SIZE 8096 //any reason about this amt? 
#define forever for(;;)

char WWW_PATH[PATH_MAX];
int PORT = 8080;
int BACKLOG = 10;

void processArguments(int argc, char **argv) {
	int opt;
	
	while (~(opt = getopt(argc, argv, "d:D:p:P:hH"))) { 
		switch(opt) {
			case 'd':
			case 'D':
				strcpy(WWW_PATH,optarg);
				break;
			case 'p':
			case 'P':
				sscanf(optarg,"%d",&PORT);
				break;
			case 'h':
			case 'H':
				printf("hint: naivehttpd -D/var/www/html -p80\n\n"
					"\tNaive HTTPD is a naive web server\n\n");
				break;
		}
    }
} 

void doResponse(int socketfd) {
	
	static char buffer[BUFFER_SIZE+1];
	
	printf("Accepted one hit!\n");
	sprintf(buffer,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
	write(socketfd,buffer,strlen(buffer));
	sprintf(buffer,"<h1>hOi!</h1>\n");
	write(socketfd,buffer,15);
	/* 
	if (send(socketfd, "Hello, world!\n", 14, 0) == -1) {
		perror("Error: Sending response \n Reason: ");
		close(socketfd);
		exit(EXIT_SUCCESS);
	}
	*/ 
}

int main(int argc, char **argv) {
	
	// static variable will initialised by filling zeros
	static struct sockaddr_in cli_addr; 
	static struct sockaddr_in srv_addr; 
	int listenfd, acceptfd; /* listenfd is the socket used for listening */
	
	// processing configurations and arguments
	processArguments(argc, argv);
	
	printf("Naive HTTPD will listening port %d.\n", PORT);
	printf("Serve at path: %s\n\n", WWW_PATH);
	
	if (chdir(WWW_PATH) == -1) { 
		printf("Error: Can not change to directory '%s'\n", WWW_PATH);
		perror("Reason: ");
		exit(EXIT_FAILURE);
	}
	
	if (PORT < 0 || PORT > 65535) {
		printf("Error: Invalid port number '%d', should be in (0~65535)\n", PORT);
		exit(EXIT_FAILURE);
	}
	
	// open socket and start listening
	if ((listenfd = socket(AF_INET, SOCK_STREAM,0)) < 0) {
		perror("Error: Socket can not be created\n Reason: ");
		exit(EXIT_FAILURE);
	}
	
	srv_addr.sin_family = AF_INET; // IPv4 Socket
	srv_addr.sin_port = htons(PORT); // бе\_(е─)_/бе
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // should Listening at 0.0.0.0 (INADDR_ANY)
	
	if (bind(listenfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) <0) {
		perror("Error: Socket bind failed \n Reason: ");
		exit(EXIT_FAILURE);
	} 
	
	if (listen(listenfd, BACKLOG) < 0) {
		perror("Error: Socket listen failed \n Reason: ");
		exit(EXIT_FAILURE);
	}
	
	printf("Server started.\n");
	
	forever {
		int addrLen = sizeof(cli_addr);
		if ((acceptfd = accept(listenfd, (struct sockaddr *)&cli_addr, &addrLen)) < 0) {
			perror("Error: Socket accept failed \n Reason: ");
			exit(EXIT_FAILURE);
		}
		if(fork() < 0) {
			perror("Error: Fork failed \n Reason: ");
			exit(EXIT_FAILURE);
		} else {
			// is there any problem with pid?
			doResponse(acceptfd);
			close(acceptfd);
		}
	}
	
	
	return 0;

}
