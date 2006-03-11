GBROOT = ../..
include ${GBROOT}/common.mk
INCL += -I../dbload

O = extFileUpdate.o raInfoTbl.o ../dbload/dbLoadOptions.o ../dbload/dbLoadPartitions.o

PROGS = $(BINARCH)/extFileUpdate ${BINDIR}/gbExtFileUpdate

all: ${PROGS}

${PROGS}: ${O}

clean:
	rm -f $(PROGS) $O
