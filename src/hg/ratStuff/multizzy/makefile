kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = multizzy.o

multizzy: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/multizzy $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/multizzy${EXE}

clean:
	rm -f $O
