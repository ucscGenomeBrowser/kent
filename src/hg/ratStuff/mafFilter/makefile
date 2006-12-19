include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mafFilter.o

mafFilter: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/mafFilter $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/mafFilter${EXE}
