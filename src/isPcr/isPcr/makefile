include ../../inc/common.mk

L = -lm $(SOCKETLIB)
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = isPcr.o

isPcr: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/isPcr $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/isPcr${EXE}

clean:
	rm -f $O
