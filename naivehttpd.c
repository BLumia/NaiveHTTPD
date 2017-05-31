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
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "httpdutils.h"
#include "socketwrapper.h"

#define PATH_MAX 4096 
#define MAXEVENTS 64
#define BUFFER_SIZE 8096 // 8 KB
#define forever for(;;)

char WWW_PATH[PATH_MAX];
int PORT = 8080;
extern int LOG_LEVEL;
extern FileType typeArr[];
extern StatusCode statusArr[];

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

int create_and_bind(int portNum) {
    static struct sockaddr_in srv_addr; 
    int listenfd;
    // open socket and start listening
	if ((listenfd = Socket(AF_INET, SOCK_STREAM,0)) < 0) {
		exit(EXIT_FAILURE);
	}
	
	srv_addr.sin_family = AF_INET; // IPv4 Socket
	srv_addr.sin_port = htons(portNum); 
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // should Listening at 0.0.0.0 (INADDR_ANY)
	
	if (bind(listenfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) <0) {
		perror("Error: Socket bind failed \n Reason");
		exit(EXIT_FAILURE);
	} 
    return listenfd;
}

static const char reply[] =
"HTTP/1.0 200 OK\r\n"
"Content-type: text/html\r\n"
"Connection: close\r\n"
"Content-Length: 4\r\n"
"\r\n"
"200\n"
;

int main (int argc, char *argv[]) {
    int listenfd, s;
    int epollfd;
    struct epoll_event event;

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

    listenfd = create_and_bind(PORT);
    if (listenfd == -1) abort ();

    setNonblocking(listenfd);

    if (listen(listenfd, SOMAXCONN) < 0) { // replace BACKLOG as SOMAXCONN, defined in linux kernel
		perror("Error: Socket listen failed \n Reason");
		exit(EXIT_FAILURE);
	}

	epollfd = epoll_create1(0);
	if (epollfd < 0) {
		perror("Error: Epoll create failed \n Reason");
		exit(EXIT_FAILURE);
	} 

    event.data.fd = listenfd;
    event.events = EPOLLIN | EPOLLET;
    s = epoll_ctl (epollfd, EPOLL_CTL_ADD, listenfd, &event);
    if (s == -1) {
        perror ("epoll_ctl");
        abort ();
    }

    /* Buffer where events are returned */
    struct epoll_event activeEvtPool[MAXEVENTS];

    /* The event loop */
    forever {
        int evtCnt = epoll_wait (epollfd, activeEvtPool, MAXEVENTS, -1);
        for (int i = 0; i < evtCnt; i++) {
            int events = activeEvtPool[i].events;

            if ((events & EPOLLERR) || (events & EPOLLHUP) || (!(events & EPOLLIN))) {
                /* An error has occured on this fd, or the socket is not
                * ready for reading (why were we notified then?) */
                fprintf (stderr, "epoll error. events=%u\n", events);
                close (activeEvtPool[i].data.fd);
                continue;
            } else if (listenfd == activeEvtPool[i].data.fd) {
                /* We have a notification on the listening socket, which
                * means one or more incoming connections. */
                handleAccept(epollfd, listenfd);
            } else {
                /* We have data on the fd waiting to be read. Read and
                * display it. We must read whatever data is available
                * completely, as we are running in edge-triggered mode
                * and won't get a notification again for the same
                * data. */
                int done = 0;

                RequestHeader* reqHdr;
	            reqHdr = readRequestHeader(activeEvtPool[i].data.fd);
                if (reqHdr == NULL) {
                    // if cause any error, readRequestHeader will close that fd and return NULL
                    // that means we dont need an extra `close(fd)`
                    continue;
                }

                while (1) {
                    ssize_t count;
                    char buf[512];

                    count = read (activeEvtPool[i].data.fd, buf, sizeof buf);
                    if (count == -1) {
                        /* If errno == EAGAIN, that means we have read all
                        * data. So go back to the main loop. */
                        if (errno != EAGAIN) {
                            perror ("read");
                            done = 1;
                        }
                        break;
                    } else if (count == 0) {
                        /* End of file. The remote has closed the
                        * connection. */
                        done = 1;
                        break;
                    }

                    /* Write the reply to connection */
                    printf("Writing request: %s\n", reqHdr->url);
                    doResponse(activeEvtPool[i].data.fd, reqHdr);
                }

                if (done) {
                    printf ("Closed connection on descriptor %d\n",
                    activeEvtPool[i].data.fd);

                    /* Closing the descriptor will make epoll remove it
                    * from the set of descriptors which are monitored. */
                    free(reqHdr); // readRequestHeader alloc, need this
                    close (activeEvtPool[i].data.fd);
                }
            }
        }
    }

    close (listenfd);

    return 0;
}
