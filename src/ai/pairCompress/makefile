include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = pairCompress.o

pairCompress: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pairCompress $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/pairCompress

clean:
	rm -f $O
