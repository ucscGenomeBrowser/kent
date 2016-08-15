kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = lineRange.o

lineRange: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/lineRange $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/lineRange${EXE}

clean:
	rm -f $O
