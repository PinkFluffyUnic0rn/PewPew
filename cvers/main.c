#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cv.h>
#include <highgui.h>

#define STATE_SHOT 1
#define STATE_WAIT 0

#define ARROW_LEFT 	65361
#define ARROW_UP 	65362
#define ARROW_RIGHT	65363
#define ARROW_DOWN 	65364
#define ESC			27
#define SPACE       32
#define KEY_P		112

struct Aim {
	IplImage *img;
	CvPoint pos;
};

//Gets Value from HSV
unsigned char* getvals(IplImage *img) 
{
	int x, y, chans, w, h;
	unsigned char *data, *out;

	h = img->height;
	w = img->width;
	chans = img->nChannels;
	out = malloc(sizeof(unsigned char) * w * h);
	data = (unsigned char*)img->imageData;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			int r, g, b, v;

			b = data[chans * (y * w + x) + 0];
			g = data[chans * (y * w + x) + 1];
			r = data[chans * (y * w + x) + 2];
	
			v = b;
			v = (g > v)? g : v;
			v = (r > v)? r : v;
			
			out[y * w + x] = v;
		}
	}

	return out;
}

int getshot(IplImage *img, unsigned char *vdata, CvPoint *shot)
{
	int x, y, chans, w, h;
	unsigned char *data;
	int n = 0;

	h = img->height;
	w = img->width;
	chans = img->nChannels;
	data = (unsigned char*)img->imageData;

	shot->x = 0;
	shot->y = 0;
		
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (vdata[y * w + x] == 255) {
				shot->x += x;
				shot->y += y;
				n++;
			} else {
				data[chans * (y * w + x) + 0] = 0;
				data[chans * (y * w + x) + 1] = 0;
				data[chans * (y * w + x) + 2] = 0;
			}
		}
	}
	
	if (n > 0) {
		shot->x /= n;
		shot->y /= n;
	}

	return n;
}


void cleartbl(IplImage *table, int v)
{
	int x, y, chans, w, h, i;
	unsigned char *data;

	h = table->height;
	w = table->width;
	chans = table->nChannels;
	data = (unsigned char*)table->imageData;

		
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			for (i = 0; i < chans; i++) { 
				data[chans * (y * w + x) + i] = v;
			}
		}
	}
}

void drawaim(struct Aim *aim, IplImage *table)
{
	int x, y, chans, w, h, tw, i, ox, oy;
	unsigned char *tdata, *adata;

	h = aim->img->height;
	w = aim->img->width;
	tw = table->width;
	ox = aim->pos.x;
	oy = aim->pos.y;
	chans = table->nChannels;
	adata = (unsigned char*)aim->img->imageData;
	tdata = (unsigned char*)table->imageData;

		
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			for (i = 0; i < chans; i++) { 
				tdata[chans * ((y + oy) * tw + x + ox) + i] = adata[chans * (y * w + x) + i];
			}
		}
	}
}

int main(int argc, char **argv)
{
	int dev, flag, maxshots, nshots, state;
	int tw = 1024, th = 768;
	CvCapture *capt;
	CvPoint *shots;
	struct Aim aim;
	IplImage *table;

	dev = 0;
	if (argc > 1)
		dev = atoi(argv[1]);

	capt = cvCaptureFromCAM(dev);


	flag = 1;
	maxshots = 1024;
	nshots = 0;
	state = STATE_WAIT;

	shots = malloc(sizeof(CvPoint) * maxshots);


	aim.img = cvLoadImage("aim.png", CV_LOAD_IMAGE_COLOR);
	printf("w=%d h=%d chans=%d\n", aim.img->width, aim.img->height, aim.img->nChannels);
	aim.pos.x = 0;
	aim.pos.y = 0;
	table = cvCreateImage(cvSize(tw, th), aim.img->depth, aim.img->nChannels);

	cvNamedWindow("Flat", CV_WINDOW_AUTOSIZE);
	cvNamedWindow("Blackened", CV_WINDOW_AUTOSIZE);
	cvNamedWindow("Table", CV_WINDOW_FULLSCREEN);


	while (flag) {
		int key, i, n;
		unsigned char *vdata;
		CvPoint shot;
		IplImage *frame, *copy;

		frame = cvQueryFrame(capt);		
		copy = cvCloneImage(frame);	


		cleartbl(table, 128);
		drawaim(&aim, table);
		cvShowImage("Table", table);

		vdata = getvals(copy);
		n = getshot(copy, vdata, &shot);
		if (n > 0 && state == STATE_WAIT) {
			shots[nshots] = shot;
			state = STATE_SHOT;
			nshots++;
			if (nshots == maxshots) {
				maxshots *= 2;
				shots = realloc(shots, sizeof(CvPoint) * maxshots);
			}
		} else if (n == 0) {
			state = STATE_WAIT;
		}


		for (i = 0; i < nshots; i++) {
			cvCircle(copy, shots[i], 5, CV_RGB(0, 255, 0), 2, 8, 0);
		}

		cvShowImage("Flat", frame);
		cvShowImage("Blackened", copy);

		key = cvWaitKey(10);

		switch (key) {
		case ESC:
			flag = 0;
			break;

		default:
			if (key != -1)
				printf("keycode=%d\n", key);;
		}

	}
	
	return 0;
}

