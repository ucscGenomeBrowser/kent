include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = wigTestMaker.o

wigTestMaker: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/wigTestMaker $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/wigTestMaker${EXE}

clean:
	rm -f $O
