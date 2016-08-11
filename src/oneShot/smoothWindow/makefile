kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = smoothWindow.o

smoothWindow: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/smoothWindow $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/smoothWindow${EXE}

clean:
	rm -f $O
