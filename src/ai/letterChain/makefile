include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = letterChain.o

letterChain: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/letterChain $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/letterChain

clean:
	rm -f $O
