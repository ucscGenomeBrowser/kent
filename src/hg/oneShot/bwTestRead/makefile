kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bwTestRead.o

bwTestRead: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bwTestRead $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bwTestRead${EXE}

clean:
	rm -f $O
