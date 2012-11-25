/* net.c*/
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
 #include <winsock2.h>
 #include <ws2tcpip.h>
 #define close closesocket
#else
 #include <fcntl.h>
 #include <netdb.h>
 #include <sys/socket.h>
 #include <sys/types.h>
 #include <unistd.h>
#endif

#include "config.h"
#include "net.h"
#include "packet.h"

static const char *errmsg;
static int fd = -1;

const char *net_geterror(void)
{
	const char *s = errmsg;
	
	errmsg = 0;
	return s ? s : "no error has occured";
}

#ifdef _WIN32
static void setnonblock(int fd)
{
	unsigned long n = 1;

	ioctlsocket(fd, FIONBIO, &n);
}
#else
 #define setnonblock(fd) \
	(fcntl((fd), F_SETFL, O_NONBLOCK))
#endif

int net_init(const char *hostaddr, const char *port)
{
	struct addrinfo *sinfo, *ptr;
	struct addrinfo hints;
	int r;
#ifdef _WIN32
	WSADATA wd;

	if (WSAStartup(MAKEWORD(1,1), &wd)) {
		errmsg = "WSAStartup failed";
		return -1;
	}
#endif
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	if ((r = getaddrinfo(hostaddr, port, &hints, &sinfo))) {
		errmsg = gai_strerror(r);
		return -1;
	}
	for (ptr = sinfo; ptr; ptr = ptr->ai_next) {
		r = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (r < 0)
			continue;
		if (!connect(r, ptr->ai_addr, ptr->ai_addrlen)) {
			freeaddrinfo(sinfo);
			fd = r;
			setnonblock(fd);
			return 0;
		}
		close(r);
	}
	freeaddrinfo(sinfo);
	errmsg = "unable to bind socket";
	return -1;
}

int net_send(const void *p, size_t n)
{
	static unsigned long frame_id;
	unsigned char packet[PKT_SIZE];
	unsigned long id = frame_id++;
	unsigned long i = 0;
	ssize_t r;

	do {
		size_t k;

		packet[PKT_TYPE] = PKT_TFRAME;
		pack32(packet + PKT_FID, id);
		pack32(packet + PKT_SEQ, i);
		i++;
		k = n > PKT_SIZE - PKT_DATA ? PKT_SIZE - PKT_DATA : n;
		memcpy(packet + PKT_DATA, p, k);
		k += PKT_DATA;
		if ((r = send(fd, packet, k, 0)) < PKT_DATA) {
			errmsg = strerror(errno);
			return -1;
		}
		p = (char *) p + r - PKT_DATA;
		n -= r - PKT_DATA;
	} while (r == sizeof packet);
	if (n) {
		errmsg = "error sending data";
		return -1;
	}
	return 0;
}

struct pk {
	int type;
	unsigned long fid;
	unsigned long seq;
	unsigned char data[PKT_SIZE - PKT_DATA];
	int len;
	struct pk *next;
};

static struct pk *cframe;

static void freelist(struct pk *pk)
{
	while (pk) {
		struct pk *next = pk->next;

		free(pk);
		pk = next;
	}
}

void net_cleanup(void)
{
	freelist(cframe);
	if (fd >= 0)
		close(fd);
#ifdef _WIN32
	WSACleanup();
#endif
}

static time_t timeout;
static int tset;

double net_twaiting(void)
{
	return tset ? difftime(time(0), timeout) : -1;
}

static int recv_aux(void **buf, size_t *n);

void *net_recv(int *status, size_t *n)
{
	void *r = 0;
	int s;

	if (!tset) {
		timeout = time(0);
		tset = 1;
	}
	s = NET_SBLOCK;
	while ((*status = recv_aux(&r, n)) != NET_SBLOCK
	 && *status != NET_SERROR)
		s = *status;
	if (s != NET_SBLOCK && s != NET_SERROR)
		timeout = time(0);
	*status = s;
	if (s == NET_SPENDING)
		goto reset;
	return r;
reset:
	freelist(cframe);
	cframe = 0;
	return r;
}

static int complete(const struct pk *pk)
{
	unsigned long seq;

	for (seq = 0; pk->next; seq++) {
		if (pk->seq != seq)
			return 0;
		pk = pk->next;
	}
	if (pk->seq != seq)
		return 0;
	return pk->len != PKT_SIZE - PKT_DATA;
}

static size_t length(const struct pk *pk)
{
	size_t r = 0;

	while (pk) {
		r += pk->len;
		pk = pk->next;
	}
	return r;
}

static size_t yield(unsigned char *to, const struct pk *pk)
{
	unsigned char *savep = to;

	while (pk) {
		memcpy(to, pk->data, pk->len);
		to += pk->len;
		pk = pk->next;
	}
	return to - savep;
}

static int insert(struct pk **list, struct pk *pk)
{
	while (*list && pk->seq >= (*list)->seq) {
		if (pk->seq == (*list)->seq) {
			free(pk);
			return -1;
		}
		list = &(*list)->next;
	}
	pk->next = *list;
	*list = pk;
	return 0;
}

static int getpacket(const char **e, struct pk *pk)
{
	unsigned char packet[PKT_SIZE];
	ssize_t r;

	if ((r = recv(fd, packet, sizeof packet, 0)) < 0) {
		*e = strerror(errno);
		return errno == EAGAIN ? NET_SBLOCK : NET_SERROR;
	}
	if (r <= PKT_TYPE)
		goto epacket;
	if ((pk->type = packet[PKT_TYPE]) != PKT_TFRAME)
		return 0;
	if (r < PKT_DATA)
		goto epacket;
	pk->fid = unpack32(packet + PKT_FID);
	pk->seq = unpack32(packet + PKT_SEQ);
	pk->len = r - PKT_DATA;
	memcpy(pk->data, packet + PKT_DATA, pk->len);
	return NET_SNONE;
epacket:
	*e = "erroneous packet data received";
	return NET_SERROR;
}

#define MEM_ERR "memory error"

int recv_aux(void **buf, size_t *n)
{
	const char *e;
	struct pk *pk;
	int r;

	if (!(pk = malloc(sizeof *pk))) {
		errmsg = MEM_ERR;
		return NET_SERROR;
	}
	if ((r = getpacket(&e, pk))) {
		errmsg = e;
		goto out;
	}
	switch (pk->type) {
	case PKT_TREFUSED:
		r = NET_SREFUSED;
		goto out;
	case PKT_TWAITING:
		r = NET_SPENDING;
		goto out;
	case PKT_TFRAME:
		break;
	default:
		errmsg = "unknown packet format";
		r = NET_SERROR;
		goto out;
	}
	r = NET_SNONE;
	if (!cframe || pk->fid == cframe->fid) {
		insert(&cframe, pk);
		return r;
	}
	if (pk->fid < cframe->fid)
		goto out;
	if (complete(cframe)) {
		if (!(*buf = malloc(length(cframe)))) {
			errmsg = MEM_ERR;
			r = NET_SERROR;
		} else {
			*n = yield(*buf, cframe);
		}
	}
	freelist(cframe);
	pk->next = 0;
	cframe = pk;
	return r;
out:
	free(pk);
	return r;
}
