CC = gcc
all: 
	$(CC) -o naivehttpd.elf -Wall naivehttpd.c -lpthread -std=c11
clean:
	rm naivehttpd.elf
