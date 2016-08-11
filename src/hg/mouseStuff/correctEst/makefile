kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = correctEst.o

correctEst: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/correctEst $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/correctEst${EXE}

clean::
	rm -f ${O}
