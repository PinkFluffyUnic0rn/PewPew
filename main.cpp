#include "opencv2/opencv.hpp"
#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h> 

#include <iostream>
#include <stdlib.h>
#include <stdio.h>

using namespace cv;
using namespace std;

uchar thres[3][2] = { {200, 255}, {0, 175}, {0, 175} };

#define STATE_SHOT 1
#define STATE_WAIT 0

#define ARROW_LEFT 	65361
#define ARROW_UP 	65362
#define ARROW_RIGHT	65363
#define ARROW_DOWN 	65364
#define ESC		27
#define SPACE           32

struct Shot
{
	Shot(Mat *i, double o)
		: img(*i), offset(o) {}

	Mat img;
	double offset;
};

bool compare_y( Point2f a, Point2f b )
{
	return ( a.y > b.y ) ? true : false;	
}

void *cmdThread(void *a)
{
	uchar tmp[3][2];
	uint tmpPause;

//	ALLEGRO_SAMPLE *march = al_load_sample("march.wav");
//	al_play_sample( march, 0.75f, 0.0f, 1.0f, ALLEGRO_PLAYMODE_ONCE, NULL );
	
	while (true) {
		scanf("%d:%d %d:%d %d:%d\n",
			&(tmp[0][0]), &(tmp[0][1]),
			&(tmp[1][0]), &(tmp[1][1]),
			&(tmp[2][0]), &(tmp[2][1]));

		thres[0][0] = (tmp[0][0] != -1) ? tmp[0][0] : thres[0][0];
		thres[0][1] = (tmp[0][1] != -1) ? tmp[0][1] : thres[0][1];
		thres[1][0] = (tmp[1][0] != -1) ? tmp[1][0] : thres[1][0];
		thres[1][1] = (tmp[1][1] != -1) ? tmp[1][1] : thres[1][1];
		thres[2][0] = (tmp[2][0] != -1) ? tmp[2][0] : thres[2][0];
		thres[2][1] = (tmp[2][1] != -1) ? tmp[2][1] : thres[2][1];
	}
	return NULL;
}

ALLEGRO_SAMPLE *init_allegro()
{
	al_init();
	al_install_audio();
	al_init_acodec_addon();
	al_reserve_samples(10);
	
	ALLEGRO_SAMPLE *smp = al_load_sample("shot.wav");

	if (!smp) {
		printf("Cannot load file\n");
		exit(-1);	
	}

	return smp;
}

void mouse_click( int e, int x, int y, int, void *arg )
{
	vector<Point2f> *persp_quad = (vector<Point2f> *) arg;

	if ( e == CV_EVENT_LBUTTONDOWN )
		if ( persp_quad->size() < 4 )
			persp_quad->push_back( Point2f(x, y) );
}

void image_perspective( Mat &frame, Mat &out, vector<Point2f> &persp_quad )
{
	double alpha = 0.0f;
	double w = frame.cols,
		h = frame.rows;;
	Point2f inp[4];
	Point2f outp[4];

	sort( persp_quad.begin(), persp_quad.end(), compare_y );

	if ( persp_quad[0].x < persp_quad[1].x )
		swap( persp_quad[0], persp_quad[1] );

	if ( persp_quad[2].x < persp_quad[3].x )
		swap( persp_quad[2], persp_quad[3] );


	inp[0] = persp_quad[0];
	inp[1] = persp_quad[1];
	inp[2] = persp_quad[2];
	inp[3] = persp_quad[3];


	outp[0] = Point2f( w, h );
	outp[2] = Point2f( w, 0 );
	outp[1] = Point2f( 0, h );
	outp[3] = Point2f( 0, 0 );;		

	Mat m = getPerspectiveTransform( inp, outp ); 

	warpPerspective( frame, out, m, Size( w, h ) );
}
		

int main(int argc, char** argv)
{
	// Init video capture device
	int devid = 0;

	if (argc > 1)
		devid = atoi(argv[1]);

	VideoCapture capt(devid);

	if (!capt.isOpened()) {
		cout << "Error opening default cam!" << endl;
		system("pause");
		return -1;
	}
	
	// Init allegro
	ALLEGRO_SAMPLE *shot_sound = init_allegro();
//	ALLEGRO_SAMPLE *headshot_sound = al_load_sample("headshot.wav");
	

	// Create thread for calibration input
	pthread_t thr;
	pthread_create(&thr, NULL, cmdThread, NULL);

	// Create windows
	namedWindow("Blackend", CV_WINDOW_AUTOSIZE);
	namedWindow("Flat", CV_WINDOW_AUTOSIZE);
	

	Point center(capt.get(CV_CAP_PROP_FRAME_WIDTH) / 2,
		capt.get(CV_CAP_PROP_FRAME_HEIGHT) / 2);
	
	vector<Point> shots;
	vector<Point2f> persp_quad;
	
	setMouseCallback( "Flat", mouse_click, &persp_quad );
	
	int state = STATE_WAIT;
	while (true) {
		Mat frame, copy;
		int sx, sy, n;
		bool res = capt.read(frame);

		if ( persp_quad.size() == 4 )
			image_perspective( frame, frame, persp_quad );
	
		copy = frame.clone();
		if (!res) {
			cout << "Error reading frame!" << endl;
			system("pause");
			return -1;
		}

		// Aiming point for frame copy
		line(copy, center - Point(15, 0), center + Point(15, 0),
			Scalar(0, 255, 0), 2, 8, 0);
		line(copy, center - Point(0, 15), center + Point(0, 15),
			Scalar(0, 255, 0), 2, 8, 0);

		if ( persp_quad.size() != 4 )
			for ( int i = 0; i < persp_quad.size(); ++i )
			{
				int dest = (i+1 != persp_quad.size()) ? i+1 : 0;
				line( copy, persp_quad[i], persp_quad[dest],
					Scalar(0, 0, 255), 2, 8, 0 );

			}


		imshow("Flat", copy);

		// Find pixels of laser point
		sx = sy = n = 0;
		for (int y = 0; y < frame.rows; y++) {
			for (int x = 0; x < frame.cols; x++){
				uchar b = frame.data[frame.channels()*(frame.cols*y + x) + 0];
				uchar g = frame.data[frame.channels()*(frame.cols*y + x) + 1];
				uchar r = frame.data[frame.channels()*(frame.cols*y + x) + 2];

				if (r >= thres[0][0] && r <= thres[0][1]
					&& g >= thres[1][0] && g <= thres[1][1]
					&& b >= thres[2][0] && b <= thres[2][1]) {
					sx = x;
					sy = y;
					n++;
				}
				else {
					frame.data[frame.channels()*(frame.cols*y + x) + 0] = 0;
					frame.data[frame.channels()*(frame.cols*y + x) + 1] = 0;
					frame.data[frame.channels()*(frame.cols*y + x) + 2] = 0;
				}
			}
		}

		// Aiming point for frame
		line(frame, center - Point(15, 0), center + Point(15, 0),
			Scalar(0, 255, 0), 2, 8, 0);
		line(frame, center - Point(0, 15), center + Point(0, 15),
			Scalar(0, 255, 0), 2, 8, 0);

		// If shot happens
		if (n > 0 && state == STATE_WAIT) {
			Point pt(sx, sy);

			shots.push_back(pt);

			state = STATE_SHOT;
			line(frame, pt, center, Scalar(0, 255, 0), 1, 8, 0);	
			
			al_play_sample( shot_sound, 1.0f, 0.0f, 1.0f, ALLEGRO_PLAYMODE_ONCE, NULL );
			
			double offset = sqrt(pow(pt.x - center.x, 2.0) + pow(pt.y - center.y, 2.0));

//			if ( offset <= 15.0 )
//				al_play_sample( headshot_sound, 1.0f, 0.0f, 1.0f, ALLEGRO_PLAYMODE_ONCE, NULL );


			cout << shots.size() << "th shot, offset: " << offset << endl;
		} else if (n == 0)
			state = STATE_WAIT;

		// Draw all shots
		for (int i = 0; i < shots.size(); ++i )
			circle( frame, shots[i], 5, Scalar(0, 0, 255), 2, 8, 0 );
		imshow("Blackend", frame);

		// Key pressing events proccessing
		// If 'esc' key is pressed, break loop
		int key = waitKey(30);
	
		if (key == ESC) {
			cout << "esc key is pressed by user" << endl;
			break;
		} else {
			switch (key) {
			case ARROW_LEFT:
				center -= Point(1, 0);
				break;
			case ARROW_UP:
				center -= Point(0, 1);
				break;
			case ARROW_RIGHT:
				center += Point(1, 0);
				break;
			case ARROW_DOWN:
				center += Point(0, 1);
				break;
			case SPACE:
				persp_quad.clear();
				break;
			}
		
			center.x = (center.x < capt.get(CV_CAP_PROP_FRAME_WIDTH))
				? center.x : capt.get(CV_CAP_PROP_FRAME_WIDTH) - 1;
			center.y = (center.y < capt.get(CV_CAP_PROP_FRAME_HEIGHT))
				? center.y : capt.get(CV_CAP_PROP_FRAME_HEIGHT) - 1;
			center.x = (center.x > 0) ? center.x : 0;
			center.y = (center.y > 0) ? center.y : 0;
		}
	}

	al_destroy_sample(shot_sound);
	return 0;
}