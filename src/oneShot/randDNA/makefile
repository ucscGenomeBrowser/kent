include ../../inc/common.mk

O = randDNA.o
A = randDNA

${A}: $O
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/${A} $O
	strip ${HOME}/bin/${MACHTYPE}/${A}

clean:
	rm -f $O
