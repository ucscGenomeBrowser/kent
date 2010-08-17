include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

MYLIBDIR = ../../lib/${MACHTYPE}
L += ${MYSQLLIBS} -lm
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = genePredToMafFrames
O = orgGenes.o mkMafFrames.o splitMultiMappings.o finishMafFrames.o genePredToMafFrames.o

genePredToMafFrames: ${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DISTDIR}${BINDIR}/${A}${EXE} ${O} ${MYLIBS} $L

orgGenes.o: orgGenes.h orgGenes.c
mkMafFrames.o: mkMafFrames.c mkMafFrames.h orgGenes.h frameIncr.h
finishMafFrames.o: finishMafFrames.c finishMafFrames.h orgGenes.h frameIncr.h
genePredToMafFrames.o: genePredToMafFrames.c orgGenes.h mkMafFrames.h splitMultiMappings.h finishMafFrames.h
splitMultiMappings.o: splitMultiMappings.c splitMultiMappings.h orgGenes.h

compile: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} ${O} ${MYLIBS} $L

test: compile
	(cd tests && ${MAKE} test)
	rm -fr tests/output ${A}${EXE} *.o

savebak:
	savebak genePredToMafFrames  *.[ch] makefile tests/makefile tests/input

clean:
	rm -fr tests/output ${A}${EXE} *.o
