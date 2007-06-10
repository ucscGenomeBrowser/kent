include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/element.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS=-ggdb

O = atomBase.o

atomBase: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/atomBase $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/atomBase${EXE}

test:	atomBase testDir testDone
	rm -rf test/output

testDir:
	rm -rf test/output
	mkdir test/output

testDone: foo goo loo poo moo

foo goo loo poo moo:
	(cd test/input; atomBase $@.f $@.ff $@.base ../output/$@.f ../output/$@.ff ../output/$@.base)
	cmp test/expected/$@.f test/output/$@.f
	cmp test/expected/$@.ff test/output/$@.ff
	cmp test/expected/$@.base test/output/$@.base
