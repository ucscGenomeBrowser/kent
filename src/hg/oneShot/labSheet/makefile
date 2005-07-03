include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = labSheet.o

labSheet: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/labSheet $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/labSheet${EXE}

clean:
	rm -f $O
