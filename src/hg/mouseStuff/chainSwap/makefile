include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainSwap.o

chainSwap: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/chainSwap $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/chainSwap${EXE}

clean::
	rm -f ${O}
