include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = htmlCheck.o

htmlCheck: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/htmlCheck $O ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/htmlCheck

clean:
	rm -f $O
