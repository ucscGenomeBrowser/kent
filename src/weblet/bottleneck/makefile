include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = bottleneck.o

bottleneck: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/bottleneck $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/bottleneck${EXE}
