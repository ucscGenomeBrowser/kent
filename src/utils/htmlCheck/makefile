include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = htmlCheck.o

htmlCheck: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/htmlCheck $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/htmlCheck${EXE}

clean:
	rm -f $O
