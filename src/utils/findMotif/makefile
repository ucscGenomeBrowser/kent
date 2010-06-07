include ../../inc/common.mk

L += -lm ${SOCKETLIB}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = findMotif.o

findMotif: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/findMotif $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/findMotif${EXE}

compile: ${O} ${MYLIBS}
	${CC} ${COPT} -o findMotif $O ${MYLIBS} $L

clean:
	rm -f $O findMotif
