include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgGoAssociation.o

hgGoAssociation: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgGoAssociation $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgGoAssociation${EXE}

test:
	hgGoAssociation go goaPart input/test1.goa -tab=output -noLoad -limit=500
	diff -x CVS -r expected output
	rm -r output
