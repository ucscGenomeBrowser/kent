include ../../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = nibbNameFix.o

nibbNameFix: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/nibbNameFix $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/nibbNameFix${EXE}

clean:
	rm -f $O
