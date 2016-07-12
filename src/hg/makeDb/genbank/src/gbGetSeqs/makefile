kentSrc = ../../../../..
GBROOT = ../..
include ${GBROOT}/common.mk

O = seqData.o pslData.o oiData.o raData.o pepData.o gbGetSeqs.o

PROGS = $(GB_BINARCH)/gbGetSeqs

all: ${PROGS}

${PROGS}: ${O}

clean:
	rm -f $(PROGS) $O
