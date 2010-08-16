include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

MYLIBDIR = ../../lib/${MACHTYPE}
L += ${MYSQLLIBS} -lm
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = genePredSingleCover
O = geneScore.o genePredSingleCover.o

genePredSingleCover: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L

geneScore.o: geneScore.c geneScore.h
genePredSingleCover.o: genePredSingleCover.c geneScore.h

compile: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} $L

test: compile
	(cd tests && ${MAKE} test)
	rm -fr tests/output ${A}${EXE} *.o

savebak:
	savebak genePredSingleCover  *.[ch] makefile tests/makefile tests/input

clean:
	rm -fr tests/output ${A}${EXE} *.o
