include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = threadTime.o

threadTime: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/threadTime $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/threadTime

clean:
	rm -f $O
