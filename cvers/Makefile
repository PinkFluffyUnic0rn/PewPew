CC = gcc
SOURCES = main.c
CFLAGS = -Wall -g `pkg-config --cflags opencv` `pkg-config --libs opencv` \
`pkg-config --cflags --libs allegro_acodec-5.0  allegro-5.0`
BINS = pewpew

all:
	$(CC) $(CFLAGS) -o $(BINS) $(SOURCES)
