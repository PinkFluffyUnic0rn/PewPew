CC=g++
CFLAGS=-lopencv_core -lopencv_highgui -lopencv_imgproc -lpthread -lm -g \
`pkg-config --cflags --libs allegro_acodec-5.0  allegro-5.0`
SOURCES=main.cpp
BINS=pewpew

all:
	$(CC) $(SOURCES) $(CFLAGS) -o $(BINS)
