include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = pslRecalcMatch.o

pslRecalcMatch: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pslRecalcMatch $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/pslRecalcMatch

clean:
	rm -f $O
