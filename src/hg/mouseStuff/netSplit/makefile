include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netSplit.o

netSplit: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/netSplit $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/netSplit${EXE}
