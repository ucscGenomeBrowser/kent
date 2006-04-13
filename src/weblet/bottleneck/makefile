include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = bottleneck.o

bottleneck: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/bottleneck $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/bottleneck${EXE}
