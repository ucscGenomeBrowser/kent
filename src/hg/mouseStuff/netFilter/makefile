include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netFilter.o

netFilter: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/netFilter $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/netFilter${EXE}
