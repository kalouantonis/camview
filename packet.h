/* packet.h*/
#ifndef PACKET_H
#define PACKET_H

enum {
	PKT_TFRAME,
	PKT_TREFUSED,
	PKT_TWAITING
};

enum {
	PKT_TYPE = 0,
	PKT_FID  = 1,
	PKT_SEQ  = 5,
	PKT_DATA = 9,
	PKT_SIZE = 508
};

#define pack32(b, n) \
        do {                                    \
                (b)[0] = (n) & 0xff;            \
                (b)[1] = (n) >> 8 & 0xff;       \
                (b)[2] = (n) >> 16 & 0xff;      \
                (b)[3] = (n) >> 24 & 0xff;      \
        } while (0)

#define unpack32(b) \
        ((unsigned long) (b)[3] << 24 | (unsigned long) (b)[2] << 16 | \
         (unsigned long) (b)[1] <<  8 | (unsigned long) (b)[0])

#endif
