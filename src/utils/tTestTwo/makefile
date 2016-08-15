kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = tTestTwo.o

tTestTwo: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/tTestTwo $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/tTestTwo${EXE}

clean:
	rm -f $O
