kentSrc = ../..
include ../../inc/common.mk

O = randDNA.o
A = randDNA

${A}: $O
	${CC} ${COPT} -o ${BINDIR}/${A} $O
	${STRIP} ${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O}

clean:
	rm -f $O
