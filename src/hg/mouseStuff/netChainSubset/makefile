include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netChainSubset.o

netChainSubset: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/netChainSubset $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/netChainSubset${EXE}
