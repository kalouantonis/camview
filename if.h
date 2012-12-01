/* if.h*/
#ifndef IF_H
#define IF_H

#include <stddef.h>
#include <cv.h>
#include <highgui.h>
#include <cxcore.h>

typedef CvCapture if_cam;
typedef IplImage if_frame;
typedef char if_window;
typedef CvMat if_mat;

extern double if_compare(if_mat *prev_img, if_mat *curr_img);
extern void if_save_image(if_frame *frame, char* save_location);
extern void if_convert_colour(if_frame *frame, if_mat *curr_img);
extern void if_mfree(if_mat *mat);

extern if_cam *if_caminit(void);
extern if_frame *if_camquery(if_cam *handle, int width, int height); /* Use for capture from frame*/
extern void if_camrelease(if_cam *handle);

extern if_window *if_winit(const char *title, int width, int height);
extern void if_wfree(if_window *handle);
extern void if_wrender(if_window *handle, const if_frame *frame);

extern void *if_fpack(const if_frame *frame, size_t *size);
extern if_frame *if_funpack(const void *buf, size_t size);
extern if_frame *if_fload(const char *path);
extern void if_frelease(if_frame *frame);

extern void if_delay(int ms);
extern int if_keystroke(void);
#endif
