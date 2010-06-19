include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = gffPeek.o

gffPeek: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gffPeek $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/gffPeek${EXE}

clean::
	rm -f ${O}
