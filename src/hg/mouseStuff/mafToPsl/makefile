include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = mafToPsl.o

mafToPsl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/mafToPsl $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/mafToPsl${EXE}

clean::
	rm -f ${O}
