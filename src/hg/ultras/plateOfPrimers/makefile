include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = plateOfPrimers.o

plateOfPrimers: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/plateOfPrimers $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/plateOfPrimers${EXE}

clean:
	rm -f $O
