include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = aliFragScore.o

aliFragScore: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/aliFragScore $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/aliFragScore

clean:
	rm -f $O
