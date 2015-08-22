#include "opencv2/opencv.hpp"
#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h> 

#include <iostream>
#include <stdlib.h>
#include <stdio.h>

using namespace cv;
using namespace std;

#define STATE_SHOT 1
#define STATE_WAIT 0

#define ARROW_LEFT 	65361
#define ARROW_UP 	65362
#define ARROW_RIGHT	65363
#define ARROW_DOWN 	65364
#define ESC		27
#define SPACE           32
#define KEY_P		112

struct Aim
{
	Mat image;
	Point vel;
	Point pos;
	Point center;
};

uchar thres[3][2] = { {0, 100}, {125, 255}, {0, 100} };
int screen_w = 1920;
int screen_h = 1080;

struct Color
{
	Color(uchar cr, uchar cg, uchar cb) : r(cr), g(cg), b(cb) {}

	uchar b;
	uchar g;
	uchar r;
};

void init_allegro()
{
	al_init();
	al_install_audio();
	al_init_acodec_addon();
	al_reserve_samples(10);
}

void mouse_click(int e, int x, int y, int, void *arg)
{
	vector<Point2f> *persp_quad = (vector<Point2f> *) arg;

	if (e == CV_EVENT_LBUTTONDOWN)
		if (persp_quad->size() < 4)
			persp_quad->push_back(Point2f(x, y));
}

bool compare_y(Point2f a, Point2f b)
{
	return (a.y > b.y) ? true : false;	
}

void image_perspective(Mat *frame, Mat *out, vector<Point2f> *persp_quad)
{
	double alpha = 0.0f;
	double w = frame->cols, h = frame->rows;;
	Point2f inp[4], outp[4];

	sort(persp_quad->begin(), persp_quad->end(), compare_y);

	if ((*persp_quad)[0].x < (*persp_quad)[1].x)
		swap((*persp_quad)[0], (*persp_quad)[1]);

	if ((*persp_quad)[2].x < (*persp_quad)[3].x)
		swap((*persp_quad)[2], (*persp_quad)[3]);

	inp[0] = (*persp_quad)[0];
	inp[1] = (*persp_quad)[1];
	inp[2] = (*persp_quad)[2];
	inp[3] = (*persp_quad)[3];


	outp[0] = Point2f(h*screen_w/screen_h, h);
	outp[2] = Point2f(h*screen_w/screen_h, 0);
	outp[1] = Point2f(0, h);
	outp[3] = Point2f(0, 0);;		

	Mat m = getPerspectiveTransform(inp, outp); 

	warpPerspective(*frame, *out, m, Size(w, h));
}

Point2f center_point( const vector<Point2f> *v )
{
	Point2f sp;

	sp.x = sp.x = 0.0;

	for ( int i = 0; i < v->size(); ++i )
		sp += (*v)[i];

	sp.x *= 1.0 / double(v->size());
	sp.y *= 1.0 / double(v->size());

	return sp;
}

double color_dist(struct Color *c0, struct Color *c1)
{
	return sqrt(pow(c0->b - c1->b, 2.0)
		+ pow(c0->g - c1->g, 2.0)
		+ pow(c0->r - c1->r, 2.0));
}

uint *create_mask(const Mat *frame,
	const Mat *prev_frame, float coldist)
{
	uint w = frame->cols;
	uint h = frame->rows;

	// mask indices start from 1, all elements, that have 0 and h + 1 as
	// one of their indices, are filled with 0
	uint *mask = (uint *) calloc((w + 2) * (h + 2), sizeof(uint));

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			struct Color c0 (frame->data[frame->channels() * (w * y + x) + 0],
				frame->data[frame->channels() * (w * y + x) + 1],
				frame->data[frame->channels() * (w * y + x) + 2]);
			struct Color c1 (prev_frame->data[prev_frame->channels() * (w * y + x) + 0],
				prev_frame->data[prev_frame->channels() * (w * y + x) + 1],
				prev_frame->data[prev_frame->channels() * (w * y + x) + 2]);
			struct Color dc (c1.b - c0.b, c1.g - c0.g, c1.r - c0.r);

			if (color_dist(&c0, &c1) > coldist && (dc.g > 100))
				mask[(w + 2) * (y + 1) + x + 1] = 1;	
			else
				mask[(w + 2) * (y + 1) + x + 1] = 0;
		}
	}

	return mask;
}

void nearest_pixels(uint *mask, uint w, uint h)
{
	for (int y = 1; y <= h; y++) {
		for (int x = 1; x <= w; x++) {
			uint neightbours = 0;

			neightbours += mask[(w + 2) * y + x + 1];
			neightbours += mask[(w + 2) * y + x - 1];
			neightbours += mask[(w + 2) * (y - 1) + x];
			neightbours += mask[(w + 2) * (y + 1) + x];

			neightbours += mask[(w + 2) * (y + 1) + x + 1];
			neightbours += mask[(w + 2) * (y + 1) + x - 1];
			neightbours += mask[(w + 2) * (y - 1) + x + 1];
			neightbours += mask[(w + 2) * (y - 1) + x - 1];

			if (neightbours < 4)
				mask[(w + 2) * y + x] = 0;
			if (neightbours > 5)
				mask[(w + 2) * y + x] = 1;
		}
	}
}


bool find_laser_point(Mat *frame, Mat *bg, Point2f *point)
{
	uint w = frame->cols;
	uint h = frame->rows;
	uint *mask;

	mask = create_mask(frame, bg, 25.0);
	nearest_pixels(mask, w, h);

	int n = 0;
	*point = Point2f(0.0, 0.0);

	vector<Point2f> l_points;

	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			uchar r, g, b;
			
			b = frame->data[frame->channels() * (w * y + x) + 0];
			g = frame->data[frame->channels() * (w * y + x) + 1];
			r = frame->data[frame->channels() * (w * y + x) + 2];

//			if (r <= 100 && g >= 115 && b <= 100) {
			if (r > thres[0][0] && r < thres[0][1]
				&& g > thres[1][0] && g < thres[1][1]
				&& b > thres[2][0] && b < thres[2][1]) {

				l_points.push_back(Point2f(x-1, y-1));
				n++;
			} else {
				frame->data[frame->channels() * (w * y + x) + 0] = 0;	
				frame->data[frame->channels() * (w * y + x) + 1] = 0;
				frame->data[frame->channels() * (w * y + x) + 2] = 0;
			}
		}

	*point = center_point( &l_points );

	free(mask);
	return n > 0;
}

void clear_screen(Mat *mscreen)
{
	memset(mscreen->data, 0, mscreen->channels()
		* mscreen->cols * mscreen->rows * sizeof(uchar));
}

void draw_sprite(Mat *aim, Mat *mscreen, uint x_offset, uint y_offset)
{
	uint chan = mscreen->channels();
	for (int y = 0; y < aim->rows; y++)
		for (int x = 0; x < aim->cols; x++)
			if (x_offset >= 0
				&& (x_offset + aim->cols) < mscreen->cols
				&& y_offset >= 0
				&& (y_offset + aim->rows) < mscreen->rows) {
				
		//		int idx = chan * (mscreen->cols * (y + y_offset) + x + x_offset);
			//	int idx1 = chan * (aim->cols * y + x);
				
				/*mscreen->data[idx + 0] = aim->data[idx1 + 0];
				mscreen->data[idx + 1] = aim->data[idx1 + 1];
				mscreen->data[idx + 2] = aim->data[idx1 + 2];*/
				memcpy(mscreen->data + chan * ((y + y_offset) * mscreen->cols + (x + x_offset)),
					aim->data + chan * (y * aim->cols + x), sizeof(uchar) * 3);
			}
}

int in_aim(struct Aim *aim, Point2f *shot)
{
	double r = (double)aim->image.cols / 2.0;
	double x = (double)(aim->center + aim->pos).x;
	double y = (double)(aim->center + aim->pos).y;
	double x0 = (double)shot->x;
	double y0 = (double)shot->y;

	return (pow(x - x0, 2) + pow(y - y0, 2)) <= pow(r, 2);
}

int main(int argc, char** argv)
{
	// Init video capture device
	Mat bg, frame;
	struct Aim aim;
	vector<Point> shots;
	vector<Point2f> persp_quad;
	int frame_w, frame_h;
	int devid = 0;

	if (argc > 1)
		devid = atoi(argv[1]);

	VideoCapture capt(devid);

	if (!capt.isOpened()) {
		cout << "Error opening default cam!" << endl;
		system("pause");
		return -1;
	}
	
	frame_w = capt.get(CV_CAP_PROP_FRAME_WIDTH);
	frame_h = capt.get(CV_CAP_PROP_FRAME_HEIGHT);
		
	aim.image = imread("sporti_black.png", CV_LOAD_IMAGE_COLOR);
	aim.vel.x = 0;
	aim.vel.y = 0;
	aim.center.x = aim.image.cols / 2;
	aim.center.y = aim.image.rows / 2;
	aim.pos.x = 0;
	aim.pos.y = 0;
	Mat mscreen(screen_h, screen_w, CV_8UC3);
	cout <<"cols " << mscreen.cols << "raws " << mscreen.rows << endl;

	// Init allegro
	init_allegro();
	ALLEGRO_SAMPLE *shot_sound = al_load_sample("blaster.wav");
	ALLEGRO_SAMPLE *march = al_load_sample("march.wav");
	ALLEGRO_SAMPLE *headshot_sound = al_load_sample("headshot.wav");
	
	al_play_sample( march, 0.75f, 0.0f, 1.0f, ALLEGRO_PLAYMODE_ONCE, NULL );

	// Create windows
	namedWindow("Blackend", CV_WINDOW_AUTOSIZE);
	namedWindow("Flat", CV_WINDOW_AUTOSIZE);
	namedWindow("Aims", CV_WINDOW_FULLSCREEN);

	setMouseCallback( "Flat", mouse_click, &persp_quad );
	capt.read(frame);

	bg = frame.clone();

	int state = STATE_WAIT;
	while (true) {
		Mat copy;
		Point2f pt;
		bool n ;
		bool res = capt.read(frame);

		if ( persp_quad.size() == 4 )
			image_perspective( &frame, &frame, &persp_quad );

		copy = frame.clone();
		if (!res) {
			cout << "Error reading frame!" << endl;
			system("pause");
			return -1;
		}
		
		clear_screen(&mscreen);	
		draw_sprite(&(aim.image), &mscreen, (int)aim.pos.x, (int)aim.pos.y);
/*
		line(mscreen, aim.center + aim.pos - Point(15, 0), aim.center + aim.pos + Point(15, 0),
			Scalar(0, 255, 0), 2, 8, 0);
		line(mscreen, aim.center + aim.pos - Point(0, 15), aim.center + aim.pos + Point(0, 15),
			Scalar(0, 255, 0), 2, 8, 0);
		circle(mscreen, aim.center + aim.pos, aim.image.cols / 2, Scalar(0, 255, 0), 2, 8, 0);
*/
		imshow("Aims", mscreen);

		if (persp_quad.size() != 4)
			for (int i = 0; i < persp_quad.size(); ++i) {
				int dest = (i + 1 != persp_quad.size()) ? (i + 1) : 0;
				line( copy, persp_quad[i], persp_quad[dest],
					Scalar(0, 0, 255), 2, 8, 0 );
			}

		imshow("Flat", copy);

		n = find_laser_point(&frame, &bg, &pt);

		// Aiming point for frame
		Point aim_frame ((aim.center.x + aim.pos.x) * (double) frame_w / (double) screen_w,
			(aim.center.y + aim.pos.y) * (double) frame_h / (double) screen_h);
		
		line(frame, aim_frame - Point(15, 0), aim_frame + Point(15, 0),
			Scalar(0, 255, 0), 2, 8, 0);
		line(frame, aim_frame - Point(0, 15), aim_frame + Point(0, 15),
			Scalar(0, 255, 0), 2, 8, 0);

		// If shot happens
		if (n > 0 && state == STATE_WAIT) {
			shots.push_back(pt);

			state = STATE_SHOT;
			line(frame, pt, aim.pos, Scalar(0, 255, 0), 1, 8, 0);	
			
			al_play_sample(shot_sound, 1.0f, 0.0f, 1.0f, ALLEGRO_PLAYMODE_ONCE, NULL);
			
			double offset = sqrt(pow(pt.x - aim.pos.x, 2.0) + pow(pt.y - aim.pos.y, 2.0));

			if (in_aim(&aim, &pt)) {		
				aim.pos.x = rand() % frame_w;
				aim.pos.y = rand() % frame_h;
				al_play_sample(headshot_sound, 1.0f, 0.0f, 1.0f, ALLEGRO_PLAYMODE_ONCE, NULL);
			}

			cout << shots.size() << "th shot, offset: " << offset << endl;
		} else if (n == 0)
			state = STATE_WAIT;

		// Draw all shots
		for (int i = 0; i < shots.size(); ++i)
			circle(frame, shots[i], 5, Scalar(0, 0, 255), 2, 8, 0);
		
		imshow("Blackend", frame);

		aim.pos.x += aim.vel.x;
		aim.pos.y += aim.vel.y;
	
		// Key pressing events proccessing
		// If 'esc' key is pressed, break loop
		int key = waitKey(30);

		if (key == ESC) {
			cout << "esc key is pressed by user" << endl;
			break;
		} else {
			switch (key) {
/*			case ARROW_LEFT:
				aim.pos -= Point(1, 0);
				break;
			case ARROW_UP:
				aim.pos -= Point(0, 1);
				break;
			case ARROW_RIGHT:
				aim.pos += Point(1, 0);
				break;
			case ARROW_DOWN:
				aim.pos += Point(0, 1);
				break;
			case SPACE:	
				bg = copy.clone();
				break;
*/
			case KEY_P:
				persp_quad.clear();
				break;

			default:
				if (key != -1)
					cout << "keycode = " << key << endl;
			}
/*		
			aim.pos.x = (aim.pos.x < capt.get(CV_CAP_PROP_FRAME_WIDTH))
				? aim.pos.x : capt.get(CV_CAP_PROP_FRAME_WIDTH) - 1;
			aim.pos.y = (aim.pos.y < capt.get(CV_CAP_PROP_FRAME_HEIGHT))
				? aim.pos.y : capt.get(CV_CAP_PROP_FRAME_HEIGHT) - 1;
			aim.pos.x = (aim.pos.x > 0) ? aim.pos.x : 0;
			aim.pos.y = (aim.pos.y > 0) ? aim.pos.y : 0;
*/
		}
	}

//	al_destroy_sample(shot_sound);
	return 0;
}
