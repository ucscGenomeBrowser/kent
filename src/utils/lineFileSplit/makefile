include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = lineFileSplit.o

lineFileSplit: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/lineFileSplit $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/lineFileSplit

clean:
	rm -f $O
