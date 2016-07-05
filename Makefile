CC=		cc
CFLAGS+=	-g -O0
CFLAGS+=	-Wall -I. -Icompat/
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith
CFLAGS+=	-Wsign-compare

PROG=		bencode
OBJS=		bencode.o

all: ${PROG}

${PROG}: ${OBJS}
	${CC} ${CFLAGS} -o $@ $<

.c.o:
	${CC} ${CFLAGS} -c -o $@ $<
