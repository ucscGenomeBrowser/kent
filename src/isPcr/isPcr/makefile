include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = isPcr.o

isPcr: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/isPcr $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/isPcr

clean:
	rm -f $O
