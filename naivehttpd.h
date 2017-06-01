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

void processArguments(int argc, char **argv);
int create_and_bind(int portNum);
