include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = headRest.o

headRest: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/headRest $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/headRest

clean:
	rm -f $O
