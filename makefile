CC = gcc
CFLAGS = -std=c11 -g -Wall -pedantic 

colonia: main.o
	$(CC) $(CFLAGS) -o colonia main.c