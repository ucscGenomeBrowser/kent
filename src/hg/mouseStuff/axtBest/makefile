include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = axtBest.o

axtBest: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/axtBest $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/axtBest${EXE}
