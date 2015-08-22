CC = gcc
SOURCES = main.c
CFLAGS = -Wall -g `pkg-config --cflags opencv` `pkg-config --libs opencv`
BINS = pewpew

all:
	$(CC) $(CFLAGS) -o $(BINS) $(SOURCES)
