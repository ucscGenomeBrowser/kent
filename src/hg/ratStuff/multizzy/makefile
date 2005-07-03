include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = multizzy.o

multizzy: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/multizzy $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/multizzy${EXE}

clean:
	rm -f $O
