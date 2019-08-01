CC = gcc
CFLAGS = -std=c11 -g -Wall -pedantic
LIBS = -lm -lncurses -lGLEW -lSDL2

# Non-portable way of obtaining the OS type ...
OSTYPE := $(shell uname)

ifeq ($(OSTYPE), Darwin)
	CFLAGS += -framework OpenGL
else ifeq ($(OSTYPE), Linux)
	CFLAGS += -lGL
else
# TODO: Windows build support?
endif

default:
	$(CC) $(CFLAGS) -o colonia main.c $(LIBS)
