#ifndef NET_H
#define NET_H

#include <stddef.h>

enum {
	NET_SNONE,
	NET_SERROR,
	NET_SBLOCK,
	NET_SPENDING,
	NET_SREFUSED
};

extern int net_init(const char *hostaddr, const char *port);

extern int net_send(const void *ptr, size_t size);

extern void *net_recv(int *status, size_t *n);

extern void net_cleanup(void);

extern const char *net_geterror(void);

extern double net_twaiting(void);

#endif
