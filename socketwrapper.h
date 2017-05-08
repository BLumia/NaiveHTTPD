// Naive HTTPD
// BLumia (c) 2017
// WTFPL
#include <errno.h>

int Socket(int family, int type, int protocol) {
    int n;
    if((n = socket(family, type, protocol)) < 0) {
	    perror("Error: Socket can not be created\n Reason");
	    return -1;
    }
    return n;
}

int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr) {
    int n;
AGAIN:
    if ( (n = accept(fd, sa, salenptr)) < 0) {
#ifdef EPROTO
        if (errno == EPROTO || errno == ECONNABORTED)
#else
        if (errno == ECONNABORTED)
#endif
            goto AGAIN;
        else {
            perror("Error: Socket accept failed\n Reason");
            return -1;
        }
    }
    return n;
}
