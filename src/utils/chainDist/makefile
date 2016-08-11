kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =   ${MYLIBDIR}/jkweb.a

O = chainDist.o

chainDist: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/chainDist $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/chainDist${EXE}

clean:
	rm -f $O
