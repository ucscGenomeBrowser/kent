include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = wordUse.o

wordUse: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/wordUse $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/wordUse

clean:
	rm -f $O
