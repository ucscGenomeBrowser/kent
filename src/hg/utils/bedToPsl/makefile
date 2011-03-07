include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a

A = bedToPsl

O = $A.o

BIN = ${BINDIR}
PROG = ${BIN}/$A

${PROG}: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${PROG} $O $(MYLIBS) $L

$O: makefile $A.c

lib:
	cd ../lib && ${MAKE}
	cd ../../lib && ${MAKE}

test::
	cd tests && ${MAKE} test

clean::
	rm -f $O
	(cd tests && ${MAKE} clean)
