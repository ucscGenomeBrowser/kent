kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = pslMaxMap.o


pslMaxMap: $O ${MYLIBS}
	${CC} ${CFLAGS} ${COPT} -o ${BINDIR}/pslMaxMap $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/pslMaxMap${EXE}

clean:
	rm -f $O
