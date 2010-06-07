include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = pslRecalcMatch.o

pslRecalcMatch: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/pslRecalcMatch $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/pslRecalcMatch${EXE}

clean:
	rm -f $O
