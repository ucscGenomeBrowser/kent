include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainNet.o

chainNet: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/chainNet $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/chainNet${EXE}

clean::
	rm -f ${O}
