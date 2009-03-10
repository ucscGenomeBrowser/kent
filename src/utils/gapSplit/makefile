include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gapSplit.o

gapSplit: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gapSplit $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/gapSplit${EXE}

compile: ${O} ${MYLIBS}
	${CC} ${COPT} -o gapSplit $O ${MYLIBS} $L

clean:
	rm -f $O gapSplit
