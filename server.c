#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "packet.h"

struct con {
	struct sockaddr_storage sa;
	char addr[INET6_ADDRSTRLEN];
	time_t lastmsg;
	int active;
};

struct data {
	unsigned char data[PKT_SIZE];
	int size;
	struct sockaddr_storage sa;
	char addr[INET6_ADDRSTRLEN];
};

#define NELEMS(a) (sizeof (a) / sizeof ((a)[0]))

static int checktimeouts(struct con *cp, int n);
static int handle(struct con *cp, size_t n, const struct data *data, int fd);

static int init(const char *port)
{
	struct addrinfo *sinfo, *p;
	struct addrinfo hints;
	int r;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	if ((r = getaddrinfo(0, port, &hints, &sinfo))) {
		fprintf(stderr, "%s\n", gai_strerror(r));
		return -1;
	}
	for (p = sinfo; p; p = p->ai_next) {
		r = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (r < 0)
			goto err;
		if (bind(r, p->ai_addr, p->ai_addrlen) >= 0) {
			freeaddrinfo(sinfo);
			return r;
		}
		close(r);
err:
		fprintf(stderr, "%s\n", strerror(errno));
	}
	freeaddrinfo(sinfo);
	fputs("failed to bind socket\n", stderr);
	return -1;
}

static void *getaddr(const void *sa)
{
	return ((struct sockaddr *) sa)->sa_family == AF_INET ?
		(void *) &((struct sockaddr_in *) sa)->sin_addr :
		(void *) &((struct sockaddr_in6 *) sa)->sin6_addr;
}

static int getdata(struct data *pkt, int fd)
{
	struct sockaddr_storage ta;
	socklen_t al = sizeof ta;
	ssize_t r;

	if ((r = recvfrom(fd, pkt->data, sizeof pkt->data, 0,
					(struct sockaddr *) &ta, &al)) < 0) {
		fprintf(stderr, "recvfrom: %s\n", strerror(errno));
		return -1;
	}
	inet_ntop(ta.ss_family, getaddr(&ta), pkt->addr, sizeof pkt->addr);
	pkt->sa = ta;
	pkt->size = r;
	return 0;
}

static void printstatus(const char *format, ...)
{
	time_t t = time(0);
	char date[16];
	va_list ap;

	strftime(date, sizeof date, "%H:%M:%S", localtime(&t));
	printf("[%s] ", date);
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
	putchar('\n');
}

int main(int argc, char *argv[])
{
	struct con ctab[2];
	int fd;

	memset(ctab, 0, sizeof ctab);
	if ((fd = init(argc > 1 ? argv[1] : DEFAULT_PORT)) < 0)
		return EXIT_FAILURE;
	printstatus("server started; waiting for connections ...");
	for (;;) {
		struct data data;
		struct timeval tv;
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		select(fd + 1, &fds, 0, 0, &tv);
		if (FD_ISSET(fd, &fds) && !getdata(&data, fd))
			handle(ctab, NELEMS(ctab), &data, fd);
		checktimeouts(ctab, NELEMS(ctab));
	}
	close(fd);
	return 0;
}

static int sendpacket(int fd, const void *p, size_t n,
		const struct sockaddr_storage *a)
{
	return sendto(fd, p, n, 0, (const struct sockaddr *) a, sizeof *a) < 0;
}

static struct con *lookup(const struct con *cp, int n, const char *addr)
{
	while (n--) {
		if (cp->active && !strcmp(addr, cp->addr))
			return (struct con *) cp;
		cp++;
	}
	return 0;
}

static struct con *getinactive(const struct con *cp, int n)
{
	while (n--) {
		if (!cp->active)
			return (struct con *) cp;
		cp++;
	}
	return 0;
}

static struct con *getactive(const struct con *cp, int n, const struct con *ptr)
{
	while (n--) {
		if (cp != ptr && cp->active)
			return (struct con *) cp;
		cp++;
	}
	return 0;
}

int checktimeouts(struct con *cp, int n)
{
	time_t t = time(0);
	int r = -1;

	while (n--) {
		if (cp->active && difftime(t, cp->lastmsg) > CLIENT_TIMEOUT) {
			printstatus("%s: timed out", cp->addr);
			cp->active = 0;
			r = 0;
		}
		cp++;
	}
	return r;
}

int handle(struct con *cons, size_t n, const struct data *data, int fd)
{
	unsigned char m;
	struct con *cp;

	if (!(cp = lookup(cons, n, data->addr))) {
		if (!(cp = getinactive(cons, n))) {
			printstatus("%s: connection refused", data->addr);
			m = PKT_TREFUSED;
			return sendpacket(fd, &m, 1, &data->sa);
		}
		printstatus("%s: connected", data->addr);
		memcpy(cp->addr, data->addr, sizeof data->addr);
		cp->sa = data->sa;
		cp->active = 1;
		if (!getinactive(cons, n))
			printstatus("session started");
	}
	cp->lastmsg = time(0);
	if (!(cp = getactive(cons, n, cp))) {
		m = PKT_TWAITING;
		return sendpacket(fd, &m, 1, &data->sa);
	}
	return sendpacket(fd, data->data, data->size, &cp->sa);
}
