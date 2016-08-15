kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = knownToBioImage.o

knownToBioImage: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/knownToBioImage $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/knownToBioImage${EXE}

clean:
	rm -f $O
