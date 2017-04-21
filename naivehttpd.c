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

ssize_t fdgets(int socketfd, char* buffer, int size) {
	
	ssize_t received, i = 0;
	char ch = '\0';
	
	while(i < (size - 1)) {
		if (ch == '\n') break;
		received = recv(socketfd, &ch, 1, 0);
		if (received > 0) {
			if (ch == '\r') { // CR LF -> LF, CR -> LF
				received = recv(socketfd, &ch, 1, MSG_PEEK);
				if (received > 0 && ch == '\n') recv(socketfd, &ch, 1, 0);
				else ch = '\n';
			}
			buffer[i] = ch;
			i++;
		} else {
			break;
		}
	}
	
	buffer[i] = '\0';
	return i;
}

void doResponse(int socketfd) {
	
	char buffer[BUFFER_SIZE+1];
	ssize_t readSize;
	
	memset(buffer, 0, sizeof(buffer));
	
	// request
	readSize = fdgets(socketfd,buffer,BUFFER_SIZE);
	if(readSize <= 0) {
		perror("Error when reading data from request");
		close(socketfd);
		exit(EXIT_FAILURE);
	}
	// since we don't need process other (may complex) method, just return 405 and exit. (why not 501?)
	if(strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4)) {
		sprintf(buffer,"HTTP/1.0 405 Method Not Allowed\r\nContent-Type: text/html\r\nContent-Length: 4\r\n\r\n405\n");
		write(socketfd,buffer,strlen(buffer));
		printf("Forbid: Not allowed method or other reason:%s\n",buffer);
		exit(EXIT_FAILURE); // is that a kind of ... failure?
	}
	for(int i=4; i<BUFFER_SIZE; i++) {
		if(buffer[i] == ' ') {
			buffer[i] = '\0';
			break;
		}
	}
	
	// response
	printf("Accepted one hit, [%s]!\n", buffer);
	sprintf(buffer,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: 14\r\n\r\n");
	write(socketfd,buffer,strlen(buffer));
	sprintf(buffer,"<h1>hOi!</h1>\n");
	write(socketfd,buffer,15);
	
	// finally
	close(socketfd);
	exit(EXIT_SUCCESS);

}

int main(int argc, char **argv) {
	
	// static variable will initialised by filling zeros
	static struct sockaddr_in cli_addr; 
	static struct sockaddr_in srv_addr; 
	pid_t pid;
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
		perror("Error: Socket can not be created\n Reason");
		exit(EXIT_FAILURE);
	}
	
	srv_addr.sin_family = AF_INET; // IPv4 Socket
	srv_addr.sin_port = htons(PORT); // бе\_(е─)_/бе
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // should Listening at 0.0.0.0 (INADDR_ANY)
	
	if (bind(listenfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) <0) {
		perror("Error: Socket bind failed \n Reason");
		exit(EXIT_FAILURE);
	} 
	
	if (listen(listenfd, BACKLOG) < 0) {
		perror("Error: Socket listen failed \n Reason");
		exit(EXIT_FAILURE);
	}
	
	printf("Server started.\n");
	
	forever {
		int addrLen = sizeof(cli_addr);
		if ((acceptfd = accept(listenfd, (struct sockaddr *)&cli_addr, &addrLen)) < 0) {
			perror("Error: Socket accept failed \n Reason");
			exit(EXIT_FAILURE);
		}
		pid = fork();
		if(pid < 0) {
			perror("Error: Fork failed \n Reason");
			exit(EXIT_FAILURE);
		} else {
			// is there any problem with pid?
			if(!pid) { 
				// child
				printf("[%d]fork child for %d\n",pid, acceptfd);
				close(listenfd);
				doResponse(acceptfd); // never returns
			} else { 
				// parent
				printf("[%d]fork parent\n",pid);
				// TODO: maybe something wrong about this, if no Content-Length header not provided, page will infinity load for resource.
				//       but #f4c8c024 don't have this issue (instead, both parent and child process will both response that request. that's a bug). 
				//close(acceptfd); // don't close this or will get a conn reset. idk why. will figure out after the fucking final exam.
			}
		}
	}
	
	
	return 0;

}
