include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(MYLIBDIR)/element.a
CFLAGS=-ggdb

O = atomFillGaps.o

atomFillGaps: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/atomFillGaps $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/atomFillGaps${EXE}

test:	atomFillGaps testDir testDone
	rm -rf test/output

testDir:
	rm -rf test/output
	mkdir test/output

testDone: foo goo loo poo moo

foo goo loo poo moo:
	(cd test/input; atomFillGaps $@.f $@.ff $@.base ../output/$@.f ../output/$@.ff ../output/$@.base)
	cmp test/expected/$@.f test/output/$@.f
	cmp test/expected/$@.ff test/output/$@.ff
	cmp test/expected/$@.base test/output/$@.base
