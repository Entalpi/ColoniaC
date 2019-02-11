CC = gcc
CFLAGS = -std=c11 -g -Wall -pedantic 
LIBS = -lncurses

default: 
	$(CC) $(CFLAGS) $(LIBS) -o colonia main.c