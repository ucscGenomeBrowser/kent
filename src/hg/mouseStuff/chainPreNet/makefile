include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainPreNet.o

chainPreNet: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/chainPreNet $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/chainPreNet${EXE}

clean::
	rm -f ${O}
