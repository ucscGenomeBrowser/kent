include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = findMotif.o

findMotif: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/findMotif $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/findMotif${EXE}

compile: ${O} ${MYLIBS}
	${CC} ${COPT} -o findMotif $O ${MYLIBS} $L

clean:
	rm -f $O findMotif
