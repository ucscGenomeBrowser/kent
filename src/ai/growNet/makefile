include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = growNet.o

growNet: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/growNet $O ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/growNet

clean:
	rm -f $O
