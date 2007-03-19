include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = selectTrainedHits.o

selectTrainedHits: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/selectTrainedHits $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/selectTrainedHits${EXE}
