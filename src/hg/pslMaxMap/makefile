include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = pslMaxMap.o


pslMaxMap: $O ${MYLIBS}
	${CC} ${CFLAGS} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pslMaxMap $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/pslMaxMap

clean:
	rm -f $O
