all:
	g++ main.cpp -lopencv_core -lopencv_highgui -lopencv_imgproc -lpthread \
	`pkg-config --cflags --libs allegro_acodec-5.0  allegro-5.0` -lm -g
