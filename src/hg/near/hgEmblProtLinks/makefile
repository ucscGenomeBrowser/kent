include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgEmblProtLinks.o

hgEmblProtLinks: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgEmblProtLinks $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgEmblProtLinks${EXE}
