// Naive HTTPD
// BLumia (c) 2017
// WTFPL

#define BUFFER_SIZE 8096 // 8 KB

enum RequestType {UNSUPPORTED, GET, POST, PUT, DELETE /*, BLAH */}; // we only support GET.

typedef struct supportedFileType {
	char* ext;
	char* type;
} FileType;

typedef struct httpStatusCode {
	int code;
	char* desc;
} StatusCode;

typedef struct httpRequestHeader {
	enum RequestType type;
	char* url;
	int responseCode;
	// blah blah blah
} RequestHeader;

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

RequestHeader* readRequestHeader(int socketfd) {
	
	ssize_t readSize;
	RequestHeader* ret;
	ret = malloc(sizeof(RequestHeader));
	char buffer[BUFFER_SIZE+1], buffer2[BUFFER_SIZE+1];
	
	readSize = fdgets(socketfd,buffer,BUFFER_SIZE);
	if(readSize <= 0) {
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
	
	ret->url = genUrldecodedStr(&buffer[5]);
	ret->responseCode = 200;
	
	// clean up read
	read(socketfd,buffer2,BUFFER_SIZE);
	
	return ret;
} 
