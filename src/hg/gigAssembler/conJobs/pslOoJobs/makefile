kentSrc = ../../../..
include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslOoJobs.o

pslOoJobs: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/pslOoJobs $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/pslOoJobs${EXE}

clean::
	rm -f ${O}
