CC = gcc
all: 
	$(CC) -o naivehttpd.exe -Wall naivehttpd.c -lpthread -std=c11
clean:
	rm naivehttpd.exe