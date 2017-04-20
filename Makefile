CC = gcc
all: 
	$(CC) -o naivehttpd.exe naivehttpd.c
clean:
	rm naivehttpd.exe