include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mafToAxt.o

mafToAxt: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/mafToAxt $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/mafToAxt${EXE}
