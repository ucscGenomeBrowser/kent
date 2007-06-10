include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(MYLIBDIR)/element.a
CFLAGS=-ggdb

O = atomString.o

atomString: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/atomString $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/atomString${EXE}

test:	atomString testDir testDone
	rm -rf test/output

testDir:
	rm -rf test/output
	mkdir test/output

testDone: foo goo loo poo moo

foo goo loo poo moo:
	(cd test/input; atomString $@.f $@.ff $@.base ../output/$@.f ../output/$@.base)
#	cmp test/expected/$@.ff test/output/$@.ff
	cmp test/expected/$@.f test/output/$@.f
	cmp test/expected/$@.base test/output/$@.base
