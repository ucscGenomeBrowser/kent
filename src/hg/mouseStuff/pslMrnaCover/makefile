include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslMrnaCover.o

pslMrnaCover: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/pslMrnaCover $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/pslMrnaCover${EXE}

clean::
	rm -f ${O}
