include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = dupeFoo.o

dupeFoo: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/dupeFoo $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/dupeFoo${EXE}
