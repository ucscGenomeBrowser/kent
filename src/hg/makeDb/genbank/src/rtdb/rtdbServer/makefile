GBROOT = ../../..
include ${GBROOT}/common.mk

O = rtdbServer.o

PROGS = $(BINARCH)/rtdbServer

all: ${PROGS}

${PROGS}: ${O}

clean:
	rm -f $(PROGS) $O
