include ../../inc/common.mk
COPT=-ggdb

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a

A = getRna

O = $A.o

BIN = $(HOME)/bin/$(MACHTYPE)
PROG = ${BIN}/$A

${PROG}: $O $(MYLIBS)
	gcc ${COPT} -o ${PROG} $O $(MYLIBS) $L

$O: makefile $A.c

lib:
	cd ../lib; make
	cd ../../lib; make
