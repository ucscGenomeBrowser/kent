include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(MYLIBDIR)/element.a
CFLAGS=-ggdb

O = atomToSeq.o

atomToSeq: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/atomToSeq $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/atomToSeq${EXE}

test:	atomToSeq testDir testDone
	rm -rf test/output

testDir:
	rm -rf test/output
	mkdir test/output

testDone: foo goo loo poo moo

foo goo loo poo moo:
	(cd test/input; atomToSeq $@.f $@.ff $@.base ../output/$@.f ../output/$@.ff ../output/$@.base)
	cmp test/expected/$@.f test/output/$@.f
	cmp test/expected/$@.ff test/output/$@.ff
	cmp test/expected/$@.base test/output/$@.base
