CC = gcc
all: 
	$(CC) -o naivehttpd.exe -Wall naivehttpd.c -lpthread
clean:
	rm naivehttpd.exe