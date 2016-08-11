kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = wigSort.o

wigSort: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/wigSort $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/wigSort${EXE}

clean:
	rm -f $O

test:
	wigSort test.wig out.wig
	diff expected.wig out.wig
