include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = lineRange.o

lineRange: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/lineRange $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/lineRange

clean:
	rm -f $O
