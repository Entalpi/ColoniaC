CC = gcc
CFLAGS = -std=c11 -g -Wall -pedantic 
LIBS = -lm -lncurses -lGLEW -lSDL2 -lGL

default: 
	$(CC) $(CFLAGS) -o colonia main.c $(LIBS)
