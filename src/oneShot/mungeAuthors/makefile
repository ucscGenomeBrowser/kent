include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = mungeAuthors.o

mungeAuthors: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/mungeAuthors $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/mungeAuthors${EXE}

clean:
	rm -f $O
