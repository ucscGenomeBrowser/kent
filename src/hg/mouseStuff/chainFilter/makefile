include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = chainFilter.o

chainFilter: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/chainFilter $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/chainFilter${EXE}
