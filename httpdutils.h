// Naive HTTPD
// BLumia (c) 2017
// WTFPL

#define BUFFER_SIZE 8096 // 8 KB

int LOG_LEVEL = 1; // 2 = verbose, 1 = info, 0 = no log

enum RequestType {UNSUPPORTED, GET, POST, PUT, DELETE, EPOLLFD /*, BLAH */}; // we only support GET.

typedef struct supportedFileType {
	char* ext;
	char* type;
} FileType;

typedef struct httpStatusCode {
	int code;
	char* desc;
} StatusCode;

typedef struct httpRequestHeader {
	int fd;
	enum RequestType type;
	char* url;
	int responseCode;
	// blah blah blah
} RequestHeader;

char hex2char(char ch);
char *genUrldecodedStr(char *pstr);
ssize_t fdgets(int socketfd, char* buffer, int size);
int setNonblocking(int fd);
int epollEventCtl(int epollfd, int socketfd, int event, int operation, void *ptr);
void doSimpleResponse(int socketfd);
void handleAccept(int epollfd, int socketfd);
void handleRead(int epollfd, void* ptr);
void handleWrite(int epollfd, void* ptr);
RequestHeader* genRequestHeader(int socketfd);
RequestHeader* readRequestHeader(int socketfd);

char hex2char(char ch) {
	return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

char *genUrldecodedStr(char *pstr) {
	char *buf = malloc(strlen(pstr) + 1), *pbuf = buf;
	while (*pstr) {
		if (*pstr == '%') {
			if (pstr[1] && pstr[2]) {
				*pbuf++ = hex2char(pstr[1]) << 4 | hex2char(pstr[2]);
				pstr += 2;
			}
		} else if (*pstr == '+') { 
			*pbuf++ = ' ';
		} else {
			*pbuf++ = *pstr;
		}
		pstr++;
	}
	*pbuf = '\0';
	return buf;
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

int setNonblocking(int fd) {  
    int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) == -1)  
        flags = 0;  
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);  
} 

int epollEventCtl(int epollfd, int socketfd, int event, int operation, void *ptr) {
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.events = event;
	ev.data.ptr = ptr;
	int r = epoll_ctl(epollfd, operation, socketfd, &ev);
	return r;
}

void doSimpleResponse(int socketfd) {
	char buffer[BUFFER_SIZE+1];
	read(socketfd,buffer,BUFFER_SIZE); 
	sprintf(buffer,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: 4\r\n\r\n200\n");//\r\nContent-Length: 4
	write(socketfd,buffer,strlen(buffer));
	//sleep(1);
	//close(socketfd);
} 

void handleAccept(int epollfd, int socketfd) {
	struct sockaddr_in raddr;
	socklen_t rsize = sizeof(raddr);
	int acceptfd = accept(socketfd, (struct sockaddr*)&raddr, &rsize);
	if (acceptfd < 0) {
		perror("Error: Accept failed.\n Reason");
		exit(1);
	}
	struct sockaddr_in local;
	if (LOG_LEVEL >= 2) {
		printf("Accepted a connection from %s\n", inet_ntoa(raddr.sin_addr)); fflush(stdout);
	}
	setNonblocking(acceptfd);
	
	RequestHeader* reqHdr;
	reqHdr = genRequestHeader(acceptfd);
	
	epollEventCtl(epollfd, acceptfd, EPOLLIN|EPOLLET, EPOLL_CTL_ADD, reqHdr);
}

void handleRead(int epollfd, void* ptr) {
	RequestHeader* dataptr = ptr;
	int socketfd = dataptr->fd;
	
	RequestHeader* reqHdr;
	reqHdr = readRequestHeader(socketfd);
	
	free(ptr);
	
	epollEventCtl(epollfd, socketfd, EPOLLOUT|EPOLLET, EPOLL_CTL_ADD, reqHdr);
}

void handleWrite(int epollfd, void* ptr) {
	RequestHeader* dataptr = ptr;
	char buffer[BUFFER_SIZE+1];
	int socketfd = dataptr->fd;
	
	doSimpleResponse(socketfd);
	epollEventCtl(epollfd, socketfd, EPOLLIN|EPOLLOUT|EPOLLET, EPOLL_CTL_DEL, NULL);
	
	free(ptr);
    close(socketfd);
}

RequestHeader* genRequestHeader(int socketfd) {
	
	RequestHeader* ret;
	ret = malloc(sizeof(RequestHeader));
	
	ret->type = EPOLLFD;
	ret->responseCode = 403; // nya
	ret->url = NULL;
	ret->fd = socketfd;
	
	return ret;
} 

RequestHeader* readRequestHeader(int socketfd) {
	
	ssize_t readSize;
	RequestHeader* ret;
	ret = malloc(sizeof(RequestHeader));
	char buffer[BUFFER_SIZE+1], buffer2[BUFFER_SIZE+1];
	
	readSize = fdgets(socketfd,buffer,BUFFER_SIZE);
	if(readSize <= 0) {
		printf("When processing fd %d :\n", socketfd);
		perror("Error when reading data from request");
		close(socketfd);
		return NULL;
	}
	// only GET
	if(strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4)) {
		ret->type = UNSUPPORTED;
	} else {
		ret->type = GET;
	}
	
	for(int i=4; i<BUFFER_SIZE; i++) {
		// safe check for not allowed access out of the WWW_PATH
		if(buffer[i] == '.' && buffer[i+1] == '.') {
			// in fact it may cause problem if we use dots in get parameter, e.g. https://sample.domain/example.htm&hey=yooo..oo
			ret->responseCode = 403;
			strcpy(buffer,"GET /index.html");
			break;
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
	
	ret->fd = socketfd;
	ret->url = genUrldecodedStr(&buffer[5]);
	ret->responseCode = 200;
	
	// clean up read
	read(socketfd,buffer2,BUFFER_SIZE);
	
	return ret;
} 
