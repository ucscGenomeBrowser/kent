include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = trixSearch.o

trixSearch: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/trixSearch $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/trixSearch${EXE}

clean:
	rm -f $O
