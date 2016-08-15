kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = wigAsciiCrunch.o

wigAsciiCrunch: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/wigAsciiCrunch $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/wigAsciiCrunch${EXE}

clean:
	rm -f $O
