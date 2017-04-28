CC = gcc
all: 
	$(CC) -o naivehttpd.exe -Wall naivehttpd.c
clean:
	rm naivehttpd.exe