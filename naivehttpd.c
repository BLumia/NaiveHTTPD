// Naive HTTPD
// BLumia (c) 2017
// WTFPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h> //#include <getopt.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#define PATH_MAX 4096 
#define BUFFER_SIZE 8096 // 8 KB
#define forever for(;;)

char WWW_PATH[PATH_MAX];
int PORT = 8080;
int BACKLOG = 10;

typedef struct supportedFileType {
	char* ext;
	char* type;
} FileType;

typedef struct httpStatusCode {
	int code;
	char* desc;
} StatusCode;

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
	exit(EXIT_FAILURE); // is that a kind of ... failure?
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
	
	char buffer[BUFFER_SIZE+1], *contentTypeStr;
	ssize_t readSize, bufferSize;
	int filefd;
	
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
		printf("Forbid: Not allowed method or other reason.\n");
		fireError(socketfd, 405);
	}
	for(int i=4; i<BUFFER_SIZE; i++) {
		// safe check for not allowed access out of the WWW_PATH
		if(buffer[i] == '.' && buffer[i+1] == '.') {
			// in fact it may cause problem if we use dots in get parameter, e.g. https://sample.domain/example.htm&hey=yooo..oo
			printf("Forbid: Not allowed fetching path.\n");
			fireError(socketfd, 403);
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
	
	filefd = open(&buffer[5],O_RDONLY); // hey, we should uridecode ya.
	if(filefd == -1) {
		fireError(socketfd, 403); // or maybe 404?
	}
	close(filefd); // do it tomorrow
	
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
