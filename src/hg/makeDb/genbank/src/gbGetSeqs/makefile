GBROOT = ../..
include ${GBROOT}/common.mk

O = seqData.o pslData.o oiData.o raData.o pepData.o gbGetSeqs.o

PROGS = $(BINARCH)/gbGetSeqs

all: ${PROGS}

${PROGS}: ${O}

clean:
	rm -f $(PROGS) $O
