// Naive HTTPD
// BLumia (c) 2017
// WTFPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> 
#include <fcntl.h>
#include <unistd.h> 
#include <getopt.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

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
	
	while (~(opt = getopt(argc, argv, "d:D:p:P:hHvV"))) { 
		switch(opt) {
			case 'd': case 'D':
				strcpy(WWW_PATH,optarg);
				break;
			case 'p': case 'P':
				sscanf(optarg,"%d",&PORT);
				break;
			case 'h': case 'H':
				printf("hint: naivehttpd -D/var/www/html -p80\n\n"
					"\tNaive HTTPD is a naive web server\n\n");  fflush(stdout);
				break;
			case 'v': case 'V':
				LOG_LEVEL = 2;
				printf("Verbose log enabled\n"); fflush(stdout);
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
	ssize_t bufferSize;
	int filefd;
	
	memset(buffer, 0, sizeof(buffer));
	
	// request
	RequestHeader* reqHdr;
	reqHdr = readRequestHeader(socketfd);
	
	if(reqHdr->type != GET) {
		printf("Forbid: Not allowed method or other reason.\n"); fflush(stdout);
		fireError(socketfd, 405); return;
	}
	
	if(reqHdr->responseCode != 200) {
		printf("Forbid: Not allowed fetching path.\n"); fflush(stdout);
		fireError(socketfd, 403); return;
	}
	
	sprintf(buffer, "%s", reqHdr->url);
	bufferSize = strlen(buffer);
	contentTypeStr = typeArr[0].type;
	for(int i=1; i < sizeof(typeArr); i++) {
		size_t sLen = strlen(typeArr[i].ext);
		if(!strncmp(&buffer[bufferSize-sLen], typeArr[i].ext, sLen)) {
			contentTypeStr = typeArr[i].type;
			break;
		}
	}

	char* decodedUri = reqHdr->url;
	filefd = open(decodedUri,O_RDONLY); 
	free(decodedUri);
	if(filefd == -1) {
		fireError(socketfd, 403); return;// or maybe 404?
	}
	struct stat fileStat;
	fstat(filefd, &fileStat);
	
	// response
	if (LOG_LEVEL >= 2) {
		printf("Responsed one hit, [%s]!\n", buffer); fflush(stdout);
	}
	sprintf(buffer,"HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", contentTypeStr, fileStat.st_size);
	write(socketfd,buffer,strlen(buffer));
	while ((bufferSize = read(filefd, buffer, BUFFER_SIZE)) > 0 ) {
		write(socketfd, buffer, bufferSize);
	}
	
	// finally
	//sleep(1);
	//free(reqHdr); // free outside
	close(filefd);
	//close(socketfd);
	return;
}

int main(int argc, char **argv) {
	
	// static variable will initialised by filling zeros
	static struct sockaddr_in srv_addr; 
	pthread_t tid;
	pthread_attr_t attr;
	int listenfd, epollfd;
	
	// processing configurations and arguments
	processArguments(argc, argv);
	
	printf("Naive HTTPD will listening port %d.\n", PORT);
	printf("Serve at path: %s\n\n", WWW_PATH);
	
	if (chdir(WWW_PATH) == -1) { 
		printf("Error: Can not change to directory '%s'\n", WWW_PATH);
		perror("Reason");
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
	srv_addr.sin_port = htons(PORT); // ˉ\_(�?_/ˉ
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
	
	epollfd = epoll_create1(0);
	if (epollfd < 0) {
		perror("Error: Epoll create failed \n Reason");
		exit(EXIT_FAILURE);
	} 
	setNonblocking(listenfd);
	RequestHeader* epollfdDataPtr;
	epollfdDataPtr = genRequestHeader(epollfd);
	if(LOG_LEVEL >= 2) {printf("listenfd: %d\n", listenfd); fflush(stdout);}
	epollEventCtl(epollfd, listenfd, EPOLLIN | EPOLLET, EPOLL_CTL_ADD, epollfdDataPtr);
	
	printf("Server started.\n\n"); fflush(stdout); 
	
	forever {
		const int kMaxEvents = 20;
		struct epoll_event activeEvtPool[100];
		int evtCnt = epoll_wait(epollfd, activeEvtPool, kMaxEvents, 10000/*waitms*/);
		if(LOG_LEVEL >= 1) {
			printf("epoll_wait return %d events.\n", evtCnt); fflush(stdout);
		}
		if(evtCnt == -1) {
			perror("wait");
			exit(EXIT_FAILURE);
		}
		for (int i = 0; i < evtCnt; i++) {
			int events = activeEvtPool[i].events;
			RequestHeader* reqHdr = (RequestHeader*) activeEvtPool[i].data.ptr;
			int fd = reqHdr->fd;
			if ((events & EPOLLERR) || (events & EPOLLHUP) || (!(events & EPOLLIN))) {
				printf("unknown event %u\n", events); fflush(stdout);
				close(fd);
				//should i free reqHdr?
			} else if (listenfd == fd) {
				if(LOG_LEVEL >= 2) {printf("handling Accept\n"); fflush(stdout);}
				handleAccept(epollfd, fd);
			} else {
				if(LOG_LEVEL >= 2) {printf("handling RW\n"); fflush(stdout);}
				handleRead(epollfd, reqHdr);
				handleWrite(epollfd, reqHdr);
				close(fd);
			}
		}
	}
	
	return 0;
}
