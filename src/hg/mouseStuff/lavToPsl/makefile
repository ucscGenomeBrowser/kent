include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = lavToPsl.o

lavToPsl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/lavToPsl $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/lavToPsl${EXE}
