include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = calcGap.o

calcGap: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/calcGap $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/calcGap${EXE}
