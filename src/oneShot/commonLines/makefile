kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = commonLines.o

commonLines: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/commonLines $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/commonLines${EXE}

clean:
	rm -f $O
