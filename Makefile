CC=g++
CFLAGS=-lopencv_core -lopencv_highgui -lopencv_imgproc -lpthread -lm -g
SOURCES=main.cpp
BINS=pewpew

all:
	$(CC) $(SOURCES) $(CFLAGS) -o $(BINS)
