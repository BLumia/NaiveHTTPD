// Naive HTTPD
// BLumia (c) 2017
// WTFPL

#include "httpdutils.h"

int LOG_LEVEL = 1; // 2 = verbose, 1 = info, 0 = no log

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

void fireError(int socketfd, int statusCode) {
	
	char buffer[BUFFER_SIZE+1], *statusStr;
	statusStr = statusArr[0].desc;
	for(int i=1; i < sizeof(statusArr); i++) {
		if (statusArr[i].code == statusCode) statusStr = statusArr[i].desc;
	}
	
	sprintf(buffer,"HTTP/1.0 %d %s\r\nContent-Type: text/html\r\nContent-Length: 4\r\n\r\n%d\n", statusCode, statusStr, statusCode);
	write(socketfd,buffer,strlen(buffer));
}

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

void doSimpleResponse(int socketfd) {
	char buffer[BUFFER_SIZE+1];
	read(socketfd,buffer,BUFFER_SIZE); 
	sprintf(buffer,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: 4\r\n\r\n200\n");//\r\nContent-Length: 4
	write(socketfd,buffer,strlen(buffer));
	//sleep(1);
	//close(socketfd);
} 

void doResponse(int socketfd, RequestHeader* reqHdr) {
	
	char buffer[BUFFER_SIZE+1], *contentTypeStr;
	ssize_t bufferSize;
	int filefd;
	
	memset(buffer, 0, sizeof(buffer));
	
	// request
	//RequestHeader* reqHdr;
	//reqHdr = readRequestHeader(socketfd);
	
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

void handleAccept(int epollfd, int socketfd) {
	struct epoll_event event;
	while (1) {
		struct sockaddr in_addr;
		socklen_t in_len = sizeof(in_addr);
		int acceptfd = accept (socketfd, &in_addr, &in_len);
		if (acceptfd == -1) {
			printf("errno=%d, EAGAIN=%d, EWOULDBLOCK=%d\n", errno, EAGAIN, EWOULDBLOCK);
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				/* We have processed all incoming
				* connections. */
				printf ("processed all incoming connections.\n");
				break;
			} else {
				perror ("accept");
				break;
			}
		}

		/* Make the incoming socket non-blocking and add it to the
		* list of fds to monitor. */
		setNonblocking (acceptfd);

		event.data.fd = acceptfd;
		event.events = EPOLLIN | EPOLLET;
		if (LOG_LEVEL >= 2) {
			printf("set events %u, acceptfd=%d\n", event.events, acceptfd);
		}
		
		if (epoll_ctl (epollfd, EPOLL_CTL_ADD, acceptfd, &event) == -1) {
			perror ("epoll_ctl");
			abort();
		}
	}
}

void handleRead(int epollfd, void* ptr) {

}

void handleWrite(int epollfd, void* ptr) {

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
	//read(socketfd,buffer2,BUFFER_SIZE);
	
	return ret;
} 
