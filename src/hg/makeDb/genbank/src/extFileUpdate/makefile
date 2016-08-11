kentSrc = ../../../../..
GBROOT = ../..
include ${GBROOT}/common.mk
INCL += -I../dbload

O = extFileUpdate.o raInfoTbl.o ../dbload/dbLoadOptions.o ../dbload/dbLoadPartitions.o

PROGS = $(GB_BINARCH)/extFileUpdate ${GB_BINDIR}/gbExtFileUpdate ${GB_BINDIR}/extFileTool

all: ${PROGS}

${PROGS}: ${O}

clean:
	rm -f $(PROGS) $O
