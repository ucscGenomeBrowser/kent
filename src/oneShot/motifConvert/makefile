include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = motifConvert.o

motifConvert: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/motifConvert $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/motifConvert${EXE}

clean:
	rm -f $O
