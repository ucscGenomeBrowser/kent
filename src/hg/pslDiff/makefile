kentSrc = ../..
include ../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


O = pslSets.o pslDiff.o
# pslDiff.o
A =  pslDiff

$A: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} ${L}

pslDiff.o: pslDiff.c pslSets.h

test: compile
	cd tests && ${MAKE} test
	rm -f ${A}${EXE}

clean:
	rm -f $O $B pslDiff
	cd tests && ${MAKE} clean

savebak:
	savebak pslDiff makefile *.c
