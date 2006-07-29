include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a

A = hgSelect

O = $A.o

BIN = $(HOME)/bin/$(MACHTYPE)
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
	cd tests && ${MAKE} clean
