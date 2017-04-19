// Naive HTTPD
// BLumia (c) 2017
// WTFPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //#include <getopt.h>
#include <netinet/in.h>

#define PATH_MAX 4096 

char WWW_PATH[PATH_MAX];
int PORT = 8080;

int main(int argc, char **argv) {
	
	int opt;
	
	// static variable will initialised by filling zeros
	static struct sockaddr_in cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in srv_addr; /* static = initialised to zeros */
	
	// processing configurations and arguments
	
	while (~(opt = getopt(argc, argv, "d:D:p:P:hH"))) { 
		switch(opt) {
			case 'd':
			case 'D':
				strcpy(WWW_PATH,optarg);
				break;
			case 'p':
			case 'P':
				sscanf(optarg,"%d",&PORT);
				break;
			case 'h':
			case 'H':
				printf("hint: naivehttpd -D/var/www/html -p80\n\n"
					"\tNaive HTTPD is a naive web server\n");
				break;
		}
    }
	printf("Naive HTTPD will listening port %d.\n", PORT);
	printf("Serve at path: %s\n\n", WWW_PATH);
	
	if(chdir(WWW_PATH) == -1){ 
		printf("Error: Can not change to directory '%s'\n", WWW_PATH);
		exit(EXIT_FAILURE);
	}
	
	// doWork
	printf("Hello World");
	
	return 0;

}
