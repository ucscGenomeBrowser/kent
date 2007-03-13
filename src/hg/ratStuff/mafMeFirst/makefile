include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = mafMeFirst.o

mafMeFirst: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/mafMeFirst $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/mafMeFirst${EXE}

clean:
	rm -f $O
