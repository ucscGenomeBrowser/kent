include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = cactiSine.o

cactiSine: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/cactiSine $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/cactiSine

clean:
	rm -f $O
