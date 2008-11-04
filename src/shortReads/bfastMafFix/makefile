include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bfastMafFix.o

bfastMafFix: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bfastMafFix $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bfastMafFix${EXE}

clean:
	rm -f $O
