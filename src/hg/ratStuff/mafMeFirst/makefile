include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = mafMeFirst.o

mafMeFirst: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafMeFirst $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/mafMeFirst${EXE}

clean:
	rm -f $O
