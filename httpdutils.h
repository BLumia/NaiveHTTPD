// Naive HTTPD
// BLumia (c) 2017
// WTFPL

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>

#define BUFFER_SIZE 8096 // 8 KB

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

void fireError(int socketfd, int statusCode);
char hex2char(char ch);
char *genUrldecodedStr(char *pstr);
ssize_t fdgets(int socketfd, char* buffer, int size);
int setNonblocking(int fd);
void doSimpleResponse(int socketfd);
void doResponse(int socketfd, RequestHeader* reqHdr);
void handleAccept(int epollfd, int socketfd);
void handleRead(int epollfd, void* ptr);
void handleWrite(int epollfd, void* ptr);
RequestHeader* readRequestHeader(int socketfd);
