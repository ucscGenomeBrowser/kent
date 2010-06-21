include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainStats.o

chainStats: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/chainStats $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/chainStats${EXE}

clean::
	rm -f ${O}
