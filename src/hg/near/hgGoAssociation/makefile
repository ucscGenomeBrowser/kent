include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgGoAssociation.o

hgGoAssociation: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgGoAssociation $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgGoAssociation${EXE}

test:
	hgGoAssociation go goaPart input/test1.goa -tab=output -noLoad -limit=500
	diff -x CVS -r expected output
	rm -r output

clean::
	rm -f ${O}
