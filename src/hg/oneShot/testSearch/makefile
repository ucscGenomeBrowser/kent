include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = testSearch.o

testSearch: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/testSearch $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/testSearch${EXE}

clean:
	rm -f $O
