#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cv.h>
#include <highgui.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h> 

#define STATE_SHOT 1
#define STATE_WAIT 0

#define VALUE_THRES 245
enum KEY {	
	ARROW_LEFT	= 65361,
	ARROW_UP	= 65362,
	ARROW_RIGHT	= 65363,
	ARROW_DOWN 	= 65364,
	ESC		= 27,
	SPACE		= 32,
	KEY_P		= 112,
	KEY_PLUS	= 61,
	KEY_MINUS	= 45
};

struct Aim {
	IplImage *img;
	CvPoint pos;
	CvPoint center;
	int radius;
};

struct Perspquad {
	CvPoint2D32f points[4];
	int pointc;
};

void init_allegro()
{
	al_init();
	al_install_audio();
	al_init_acodec_addon();
	al_reserve_samples(10);
}

//Gets Value from HSV
unsigned char *getvals(IplImage *img) 
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
			if (vdata[y * w + x] >= 230) {
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

		
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++)
			for (i = 0; i < chans; i++)
				data[chans * (y * w + x) + i] = v;

}

void drawaim(struct Aim *aim, IplImage *table)
{
	int x, y, chans, w, h, tw, i, ox, oy;
	unsigned char *tdata, *adata;

	h = aim->img->height;
	w = aim->img->widthStep / 3;
	tw = table->widthStep / 3;
	ox = aim->pos.x - aim->img->width / 2;
	oy = aim->pos.y - aim->img->height / 2;
	chans = table->nChannels;
	adata = (unsigned char*)aim->img->imageData;
	tdata = (unsigned char*)table->imageData;

	for (y = 0; y < h; y++)
		for (x = 0; x < aim->img->width; x++)
			for (i = 0; i < chans; i++) 
				tdata[chans * ((y + oy) * tw + x + ox) + i] = adata[chans * (y * w + x) + i];
}

void mouseclick(int e, int x, int y, int a, void *arg)
{
	struct Perspquad *perspquad = (struct Perspquad *)arg;
	
	if (e == CV_EVENT_LBUTTONDOWN && perspquad->pointc < 4)
		perspquad->points[perspquad->pointc++] = cvPoint2D32f(x, y);
}

int compare_y(const void *a, const void *b)
{
	return ((CvPoint2D32f *)b)->y - ((CvPoint2D32f *)a)->y;	
}

IplImage *persp(IplImage *in, struct Perspquad *perspquad, int w, int h)
{
	CvPoint2D32f outp[4];
	CvMat *mpersp = cvCreateMat(3, 3, CV_32FC1);
	IplImage *copy;
	
	if (perspquad->pointc < 4)
		return NULL;
	
	copy = cvCloneImage(in);	
	
	qsort(perspquad, 4, sizeof(CvPoint2D32f), compare_y);

	if (perspquad->points[0].x < perspquad->points[1].x) {
		CvPoint2D32f tmp = perspquad->points[0];
		perspquad->points[0] = perspquad->points[1];
		perspquad->points[1] = tmp;
	}

	if (perspquad->points[2].x < perspquad->points[3].x) {
		CvPoint2D32f tmp = perspquad->points[2];
		perspquad->points[2] = perspquad->points[3];
		perspquad->points[3] = tmp;	
	}
	
	outp[0] = cvPoint2D32f(in->width, in->width * h / w);
	outp[2] = cvPoint2D32f(in->width, 0);
	outp[1] = cvPoint2D32f(0, in->width * h / w);
	outp[3] = cvPoint2D32f(0, 0);

	cvGetPerspectiveTransform(perspquad->points, outp, mpersp);

	cvWarpPerspective(in, copy, mpersp,
		CV_INTER_LINEAR + CV_WARP_FILL_OUTLIERS, cvScalarAll(0));
	
	cvReleaseMat(&mpersp);

	return copy;
}

int ishit(CvPoint *shot, CvPoint *aimpos, int w, int h, int tw, int th, int d)
{

//	int x = shot->x * tw / w;
//	int y = shot->y * th / h;

	int x = shot->x;
	int y = shot->y;


	int dx = x - aimpos->x;
	int dy = y - aimpos->y;
	
	return (dx * dx + dy * dy) < (d * d);
}
int main(int argc, char **argv)
{
	int dev, flag, maxshots, nshots, state;
	int tw = 1024, th = 768;
	CvCapture *capt;
	CvPoint *shots;
	struct Aim aim;
	IplImage *table;
	struct Perspquad perspquad;
	ALLEGRO_SAMPLE *shot_sound, *hit_sound;

	dev = 0;
	if (argc > 1)
		dev = atoi(argv[1]);

	capt = cvCaptureFromCAM(dev);
	cvSetCaptureProperty(capt,CV_CAP_PROP_FRAME_WIDTH, 1024);
	cvSetCaptureProperty(capt, CV_CAP_PROP_FRAME_HEIGHT, 768); 

	init_allegro();
	shot_sound = al_load_sample("shot.wav");
	hit_sound = al_load_sample("headshot.wav");

	flag = 1;
	maxshots = 1024;
	nshots = 0;
	state = STATE_WAIT;

	shots = malloc(sizeof(CvPoint) * maxshots);


	aim.img = cvLoadImage("aim2.png", CV_LOAD_IMAGE_COLOR);
	printf("w=%d h=%d chans=%d\n", aim.img->width, aim.img->height, aim.img->nChannels);
	aim.pos.x = tw / 2;
	aim.pos.y = th / 2;
	aim.radius = 40;
	aim.center.x = tw / 2;
	aim.center.y = th / 2;
	table = cvCreateImage(cvSize(tw, th), aim.img->depth, aim.img->nChannels);

	perspquad.pointc = 0;


	cvNamedWindow("Flat", CV_WINDOW_AUTOSIZE);
	cvNamedWindow("Blackened", CV_WINDOW_AUTOSIZE);
	cvNamedWindow("Table", CV_WINDOW_FULLSCREEN);
	cvSetMouseCallback("Flat", mouseclick, &perspquad);

	while (flag) {
		int key, i, n;
		unsigned char *vdata;
		CvPoint shot;
		IplImage *frame, *copy, *flat;
	
		frame = cvQueryFrame(capt);	
		flat = cvCloneImage(frame);
		
		if (perspquad.pointc == 4) {
			copy = persp(flat, &perspquad, tw, th);
			cvReleaseImage(&flat);
			flat = copy;
		}	
			
		copy = cvCloneImage(flat);	


		cleartbl(table, 128);
		drawaim(&aim, table);
		cvShowImage("Table", table);

		vdata = getvals(copy);
		n = getshot(copy, vdata, &shot);
		free(vdata);
	
		if (n > 0 && state == STATE_WAIT) {
			al_play_sample(shot_sound, 1.0f, 0.0f, 1.0f,
				ALLEGRO_PLAYMODE_ONCE, NULL);
			
			shots[nshots] = shot;
			state = STATE_SHOT;
			nshots++;

			if (nshots == maxshots) {
				maxshots *= 2;
				shots = realloc(shots, sizeof(CvPoint) * maxshots);
			}

			if (ishit(&(shot), &aim.center,
				flat->width, flat->height, tw, th, aim.radius)) {
	
				al_play_sample(hit_sound, 1.0f, 0.0f, 1.0f,
				ALLEGRO_PLAYMODE_ONCE, NULL);
						
			//	aim.pos.x = rand() % (tw - aim.img->width) + aim.img->width / 2;
			//	aim.pos.y = rand() % (th - aim.img->height) + aim.img->height / 2;
				
			//	aim.pos.x = rand() % flat->width;
			//	aim.pos.y = rand() % flat->height;

			}
		} else if (n == 0) {
			state = STATE_WAIT;
		}


		for (i = 0; i < nshots; i++)
			cvCircle(copy, shots[i], 5, CV_RGB(0, 255, 0), 2, 8, 0);
		
		cvCircle(copy, aim.center, aim.radius, CV_RGB(255, 0, 0), 2, 8, 0);
		cvCircle(flat, aim.center, aim.radius, CV_RGB(255, 0, 0), 2, 8, 0);

		if (perspquad.pointc > 1 && perspquad.pointc < 4)
			for (i = 0; i < perspquad.pointc; ++i) {
				CvPoint p1, p2;
	
				p1 = cvPoint(perspquad.points[(i + 1) % perspquad.pointc].x,
					perspquad.points[(i + 1) % perspquad.pointc].y); 
				p2 = cvPoint(perspquad.points[i].x,
					perspquad.points[i].y); 

				cvLine(flat, p1, p2, CV_RGB(0, 255, 0), 2, 8, 0);
			}

		cvShowImage("Flat", flat);
		cvShowImage("Blackened", copy);

		cvReleaseImage(&copy);
		cvReleaseImage(&flat);

		switch ((key = cvWaitKey(10))) {
		case ESC:
			flag = 0;
			break;
		case ARROW_LEFT:
			aim.center.x -= 1;
			break;
		case ARROW_RIGHT:
			aim.center.x += 1;
			break;
		case ARROW_DOWN:
			aim.center.y += 1;
			break;
		case ARROW_UP:
			aim.center.y -= 1;
			break;
		case KEY_PLUS:
			aim.radius += 1;
			break;
		case KEY_MINUS:
			aim.radius -= 1;
			break;
		case KEY_P:
			perspquad.pointc = 0;
			break;
		default:
			if (key != -1)
				printf("keycode=%d\n", key);;
		}

	}
	
	return 0;
}

