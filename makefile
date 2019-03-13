CC = gcc
CFLAGS = -std=c11 -g -Wall -pedantic 
LIBS = -lncurses

default: 
	$(CC) $(CFLAGS) -o colonia main.c $(LIBS)
