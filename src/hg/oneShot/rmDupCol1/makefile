kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rmDupCol1.o

rmDupCol1: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/rmDupCol1 $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/rmDupCol1${EXE}

clean::
	rm -f ${O}
