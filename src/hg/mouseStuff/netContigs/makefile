include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netContigs.o

netContigs: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/netContigs $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/netContigs${EXE}
