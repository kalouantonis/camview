/* client.c*/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "config.h"
#include "if.h"
#include "net.h"

static const char *progname;

static void warn(const char *format, ...)
{
	va_list ap;

	fputs(progname, stderr);
	fputs(": ", stderr);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fputs("\n", stderr);
}

static void sendframe(const if_frame *frame)
{
	void *buf;
	size_t n;

	if (!(buf = if_fpack(frame, &n))) {
		warn("unable to convert local frame");
		return;
	}
	net_send(buf, n);
	free(buf);
}

static if_frame *recvframe(int *status)
{
	if_frame *r;
	void *buf;
	size_t n;

	if (!(buf = net_recv(status, &n)))
		return 0;
	if (!(r = if_funpack(buf, n)))
		warn("unable to convert remote frame");
	free(buf);
	return r;
}

struct ldata {
	if_cam *cam;
	if_window *wremote;
	if_window *wlocal;
	if_window *wdetect;
	if_frame *fpending;
	if_frame *fnc;
	if_frame *detect;
	if_mat *curr_img; /* Create the 2 comperative images*/
	if_mat *prev_img;
};

static int init(struct ldata *lp)
{
	if (!(lp->wremote = if_winit(RWINDOWNAME, FRAMEWIDTH, FRAMEHEIGHT))) {
		warn("unable to initialise remote display");
		return -1;
	}
	if (!(lp->wlocal = if_winit(LWINDOWNAME, FRAMEWIDTH, FRAMEHEIGHT))) {
		warn("unable to initialise local display");
		return -1;
	}
	if (!(lp->wdetect = if_winit(DWINDOWNAME, FRAMEWIDTH, FRAMEHEIGHT))) {
		warn("unable to initialise local display");
		return -1;
	}
	if (!(lp->cam = if_caminit())) {
		warn("unable to initialise camera");
		return -1;
	}
	if (!(lp->fpending = if_fload(IMAGE_PATH PENDING_FILE)))
		warn("unable to load image: %s\n", PENDING_FILE);
	if (!(lp->fnc = if_fload(IMAGE_PATH NO_CONNECT_FILE)))
		warn("unable to load image: %s\n", NO_CONNECT_FILE);
		
	if(!(lp->curr_img = cvCreateMat(lp->fnc->height, lp->fnc->width, CV_8UC1)) ||
			!(lp->prev_img = cvCreateMat(lp->curr_img->rows, lp->curr_img->cols, lp->curr_img->type)))
	{
		/* Create matrices for image comparison*/
		warn("Can not create Matrices");
		return -1;
	}
	return 0;
}

static void cleanup(struct ldata *lp)
{
	if (lp->fnc)
		if_frelease(lp->fnc);
	if (lp->fpending)
		if_frelease(lp->fpending);
	if(lp->detect)
		if_frelease(lp->detect);
	if (lp->cam)
		if_camrelease(lp->cam);
	if (lp->wremote)
		if_wfree(lp->wremote);
	if (lp->wlocal)
		if_wfree(lp->wlocal);
	if(lp->wdetect)
		if_wfree(lp->wdetect);
	if(lp->curr_img)
		if_mfree(lp->curr_img);
	if(lp->prev_img)
		if_mfree(lp->prev_img);
}

static int mainloop(struct ldata *lp)
{
	if_wrender(lp->wremote, lp->fnc);
	do {
		clock_t ms = clock();
		if_frame *f;
		int status;

		if ((f = recvframe(&status))) {
			if_wrender(lp->wremote, f);
			if_frelease(f);
		} else if (status == NET_SERROR) {
			warn("%s", net_geterror());
		} else if (status == NET_SPENDING && lp->fpending) {
			if_wrender(lp->wremote, lp->fpending);
		} else if (status == NET_SREFUSED) {
			warn("session in progress - no slots available");
			return -1;
		} else if (net_twaiting() > CLIENT_TIMEOUT && lp->fnc) {
			if_wrender(lp->wremote, lp->fnc);
		}
		if ((f = if_camquery(lp->cam, FRAMEWIDTH, FRAMEHEIGHT))) {
			if_wrender(lp->wlocal, f);
			sendframe(f);
			if_frelease(f);
		}
		ms = (clock() - ms) / (CLOCKS_PER_SEC / 1000);
		if_delay(ms < 1000 / FPS ? (1000 / FPS) - ms : 1);
	} while (if_keystroke() != EXIT_KEYCODE);
	return 0;
}

int main(int argc, char *argv[])
{
	struct ldata ldata = { 0 };
	int r;

	progname = argv[0] && argv[0][0] ? argv[0] : "camview";
	if (argc < 2) {
		fprintf(stderr, "usage: %s host-address [port]\n", progname);
		return EXIT_FAILURE;
	}
	if (net_init(argv[1], argc > 2 ? argv[2] : DEFAULT_PORT)) {
		warn("%s", net_geterror());
		return EXIT_FAILURE;
	}
	r = init(&ldata) || mainloop(&ldata) ? EXIT_FAILURE : 0;
	cleanup(&ldata);
	net_cleanup();
	return r;
}
