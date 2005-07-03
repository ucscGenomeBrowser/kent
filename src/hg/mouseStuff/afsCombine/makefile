include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = afsCombine.o

afsCombine: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/afsCombine $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/afsCombine${EXE}

clean:
	rm -f $O
