include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mafToPsl.o

mafToPsl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/mafToPsl $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/mafToPsl${EXE}
