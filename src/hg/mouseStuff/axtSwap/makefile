include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtSwap.o

axtSwap: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/axtSwap $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/axtSwap${EXE}
