kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = mungeAuthors.o

mungeAuthors: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mungeAuthors $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/mungeAuthors${EXE}

clean:
	rm -f $O
