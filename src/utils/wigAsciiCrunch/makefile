include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = wigAsciiCrunch.o

wigAsciiCrunch: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/wigAsciiCrunch $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/wigAsciiCrunch

clean:
	rm -f $O
