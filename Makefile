CC=		cc
CFLAGS+=	-g -O0
CFLAGS+=	-Wall -I. -Icompat/
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith
CFLAGS+=	-Wsign-compare

PROG=		bencode
OBJS=		bencode.o log.o torrent.o

.PHONY: clean

all: ${PROG}

${PROG}: ${OBJS}
	${CC} ${CFLAGS} -o $@ ${OBJS}

.c.o:
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -f -- ${OBJS} ${PROG}
