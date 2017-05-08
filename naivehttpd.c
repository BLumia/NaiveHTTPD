// Naive HTTPD
// BLumia (c) 2017
// WTFPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> 
#include <fcntl.h>
#include <unistd.h> //#include <getopt.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "httpdutils.h"
#include "socketwrapper.h"

#define PATH_MAX 4096 
#define BUFFER_SIZE 8096 // 8 KB
#define forever for(;;)

char WWW_PATH[PATH_MAX];
int PORT = 8080;
int BACKLOG = 10;

FileType typeArr[] = {
	{"*",   "application/octet-stream"}, // fallback
	{"gif", "image/gif"}, 
	{"jpg", "image/jpeg"}, 
	{"jpeg","image/jpeg"}, 
	{"png", "image/png"}, 
	{"ico", "image/x-icon"}, 
	{"htm", "text/html"}, 
	{"html","text/html"}, 
	{"mp3", "audio/mp3"}, // hey, PCM
	{"wav", "audio/wav"},
	{"ogg", "audio/ogg"},
	{"zip", "application/zip"}, 
	{"gz",  "application/x-gzip"}, 
	{"tar", "application/x-tar"}
};

StatusCode statusArr[] = {
	{200, "OK"}, 
	{403, "Forbidden"}, 
	{404, "Not Found"}, 
	{405, "Method Not Allowed"}, 
	{501, "Not Implemented"}
};

void processArguments(int argc, char **argv) {
	int opt;
	
	while (~(opt = getopt(argc, argv, "d:D:p:P:hH"))) { 
		switch(opt) {
			case 'd': case 'D':
				strcpy(WWW_PATH,optarg);
				break;
			case 'p': case 'P':
				sscanf(optarg,"%d",&PORT);
				break;
			case 'h': case 'H':
				printf("hint: naivehttpd -D/var/www/html -p80\n\n"
					"\tNaive HTTPD is a naive web server\n\n");
				break;
		}
    }
} 

void fireError(int socketfd, int statusCode) {
	
	char buffer[BUFFER_SIZE+1], *statusStr;
	statusStr = statusArr[0].desc;
	for(int i=1; i < sizeof(statusArr); i++) {
		if (statusArr[i].code == statusCode) statusStr = statusArr[i].desc;
	}
	
	sprintf(buffer,"HTTP/1.0 %d %s\r\nContent-Type: text/html\r\nContent-Length: 4\r\n\r\n%d\n", statusCode, statusStr, statusCode);
	write(socketfd,buffer,strlen(buffer));
}

void doResponse(int socketfd) {
	
	char buffer[BUFFER_SIZE+1], *contentTypeStr;
	ssize_t readSize, bufferSize;
	int filefd;
	
	memset(buffer, 0, sizeof(buffer));
	
	// request
	readSize = fdgets(socketfd,buffer,BUFFER_SIZE);
	if(readSize <= 0) {
		perror("Error when reading data from request");
		close(socketfd);
		return;
	}
	// since we don't need process other (may complex) method, just return 405 and exit. (why not 501?)
	if(strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4)) {
		printf("Forbid: Not allowed method or other reason.\n");
		fireError(socketfd, 405);return;
	}
	for(int i=4; i<BUFFER_SIZE; i++) {
		// safe check for not allowed access out of the WWW_PATH
		if(buffer[i] == '.' && buffer[i+1] == '.') {
			// in fact it may cause problem if we use dots in get parameter, e.g. https://sample.domain/example.htm&hey=yooo..oo
			printf("Forbid: Not allowed fetching path.\n");
			fireError(socketfd, 403);return;
		}
		// do lazy work to get path string
		if(buffer[i] == ' ') {
			buffer[i] = '\0';
			if (i == 5) { // lazy work: if requesting `/`, not work if requesting a sub folder.
				strcpy(buffer,"GET /index.html");
			}
			break;
		}
	}
	
	bufferSize = strlen(buffer);
	contentTypeStr = typeArr[0].type;
	for(int i=1; i < sizeof(typeArr); i++) {
		size_t sLen = strlen(typeArr[i].ext);
		if(!strncmp(&buffer[bufferSize-sLen], typeArr[i].ext, sLen)) {
			contentTypeStr = typeArr[i].type;
			break;
		}
	}
	
	char* decodedUri = genUrldecodedStr(&buffer[5]);
	filefd = open(decodedUri,O_RDONLY); 
	free(decodedUri);
	if(filefd == -1) {
		fireError(socketfd, 403); return;// or maybe 404?
	}
	struct stat fileStat;
	fstat(filefd, &fileStat);
	
	// response
	printf("Accepted one hit, [%s]!\n", buffer);
	sprintf(buffer,"HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", contentTypeStr, fileStat.st_size);
	write(socketfd,buffer,strlen(buffer));
	while ((readSize = read(filefd, buffer, BUFFER_SIZE)) > 0 ) {
		write(socketfd, buffer, readSize);
	}
	
	// finally
	sleep(1);
	close(filefd);
	close(socketfd);
	return;
}

int main(int argc, char **argv) {
	
	// static variable will initialised by filling zeros
	static struct sockaddr_in cli_addr; 
	static struct sockaddr_in srv_addr; 
	pthread_t tid;
	pthread_attr_t attr;
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
	if ((listenfd = Socket(AF_INET, SOCK_STREAM,0)) < 0) {
		exit(EXIT_FAILURE);
	}
	
	srv_addr.sin_family = AF_INET; // IPv4 Socket
	srv_addr.sin_port = htons(PORT); // ˉ\_(ツ)_/ˉ
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // should Listening at 0.0.0.0 (INADDR_ANY)
	
	if (bind(listenfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) <0) {
		perror("Error: Socket bind failed \n Reason");
		exit(EXIT_FAILURE);
	} 
	
	if (listen(listenfd, BACKLOG) < 0) {
		perror("Error: Socket listen failed \n Reason");
		exit(EXIT_FAILURE);
	}
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	
	printf("Server started.\n\n"); fflush(stdout); // flush, or on msys it will not automatically flush the stream.
	
	forever {
		int addrLen = sizeof(cli_addr);
		if ((acceptfd = Accept(listenfd, (struct sockaddr *)&cli_addr, &addrLen)) < 0) {
			exit(EXIT_FAILURE);
		}
		
		if (pthread_create(&tid, &attr, doResponse, acceptfd) != 0)
            perror("pthread_create");
		
	}
	
	return 0;
}
