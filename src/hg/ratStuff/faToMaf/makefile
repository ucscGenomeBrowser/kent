include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = faToMaf.o

faToMaf: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/faToMaf $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/faToMaf${EXE}

clean:
	rm -f $O
