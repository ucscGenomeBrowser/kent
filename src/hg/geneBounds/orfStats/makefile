include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = orfStats.o

orfStats: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/orfStats $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/orfStats${EXE}

clean::
	rm -f ${O}
