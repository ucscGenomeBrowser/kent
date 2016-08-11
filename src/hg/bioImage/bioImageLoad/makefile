kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = bioImageLoad.o

bioImageLoad: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bioImageLoad $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bioImageLoad${EXE}

clean:
	rm -f $O
