include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = axtCalcMatrix.o

axtCalcMatrix: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/axtCalcMatrix $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/axtCalcMatrix${EXE}
