/* if.c*/
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "if.h"

if_cam *if_caminit(void)
{
	return cvCaptureFromCAM(-1);
}

if_frame *if_camquery(if_cam *handle, int w, int h)
{
	IplImage *f, *r;

	if (!(f = cvQueryFrame(handle))
	 || !(r = cvCreateImage(cvSize(w, h), f->depth, f->nChannels)))
		return 0;
	cvResize(f, r, CV_INTER_LINEAR);
	return r;
}

void if_camrelease(if_cam *handle)
{
	cvReleaseCapture(&handle);
}

/* -------------------------------------------------------------------------- */

if_window *if_winit(const char *name, int width, int height)
{
	char *s;

	if (!(s = malloc(strlen(name) + 1)))
		return 0;
	cvNamedWindow(name, 0);
	cvResizeWindow(name, width, height);
	return strcpy(s, name);
}

void if_wfree(if_window *handle)
{
	cvDestroyWindow(handle);
	free(handle);
}

void if_wrender(if_window *handle, const if_frame *frame)
{
	cvShowImage(handle, frame);
}

/* -------------------------------------------------------------------------- */

enum {
	FRAME_QUALITY = 50	/* default: 95 */
};

void *if_fpack(const if_frame *frame, size_t *len)
{
	const int quality[] = { CV_IMWRITE_JPEG_QUALITY, FRAME_QUALITY, 0 };
	CvMat *mat;
	void *r;

	mat = cvEncodeImage(".jpg", frame, quality);
	*len = mat->step;
	if ((r = malloc(*len)))
		memcpy(r, mat->data.ptr, *len);
	cvReleaseMat(&mat);
	return r;
}

if_frame *if_funpack(const void *buf, size_t n)
{
	if_frame *r;
	if_mat *mat;
	void *tmp;

	if (!(tmp = malloc(n)))
		return 0;
	memcpy(tmp, buf, n);
	mat = cvCreateMat(1, n, CV_8UC1);
	cvSetData(mat, tmp, n);
	r = cvDecodeImage(mat, CV_LOAD_IMAGE_ANYCOLOR);
	cvReleaseMat(&mat);
	free(tmp);
	return r;
}

if_frame *if_fload(const char *path)
{
	return cvLoadImage(path, CV_LOAD_IMAGE_ANYCOLOR);
}

void if_frelease(if_frame *frame)
{
	IplImage *ptr = frame;

	cvReleaseImage(&ptr);
}

/* -------------------------------------------------------------------------- */

static int key = -1;

void if_delay(int ms)
{
	key = cvWaitKey(ms);
}

int if_keystroke(void)
{
	int r;

	if (key < 0)
		return cvWaitKey(1);
	r = key;
	key = -1;
	return r;
}
