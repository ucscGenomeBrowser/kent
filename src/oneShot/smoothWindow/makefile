include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = smoothWindow.o

smoothWindow: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/smoothWindow $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/smoothWindow

clean:
	rm -f $O
