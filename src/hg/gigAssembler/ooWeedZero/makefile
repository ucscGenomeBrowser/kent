include ../../../inc/common.mk


L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooWeedZero.o

ooWeedZero: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/ooWeedZero $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/ooWeedZero${EXE}
