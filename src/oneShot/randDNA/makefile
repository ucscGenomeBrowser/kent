include ../../inc/common.mk

O = randDNA.o
A = randDNA

${A}: $O
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/${A} $O
	${STRIP} ${HOME}/bin/${MACHTYPE}/${A}${EXE}

clean:
	rm -f $O
