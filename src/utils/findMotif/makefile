include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = findMotif.o

findMotif: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/findMotif $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/findMotif${EXE}

clean:
	rm -f $O
