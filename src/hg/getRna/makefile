include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L += $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a

A = getRna

O = $A.o

BIN = ${BINDIR}
PROG = ${BIN}/$A

${PROG}: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${PROG} $O $(MYLIBS) $L

$O: makefile $A.c

lib:
	cd ../lib && ${MAKE}
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
