include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = smoothWindow.o

smoothWindow: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/smoothWindow $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/smoothWindow${EXE}

clean:
	rm -f $O
