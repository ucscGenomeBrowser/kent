GBROOT = ../..
include ${GBROOT}/common.mk

O = seqData.o pslData.o raData.o gbGetSeqs.o

PROGS = $(BINARCH)/gbGetSeqs

all: ${PROGS}

${PROGS}: ${O}

clean:
	rm -f $(PROGS) $O
