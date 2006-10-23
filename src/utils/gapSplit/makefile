include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gapSplit.o

gapSplit: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/gapSplit $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/gapSplit${EXE}

compile: ${O} ${MYLIBS}
	${CC} ${COPT} -o gapSplit $O ${MYLIBS} $L

clean:
	rm -f $O gapSplit
