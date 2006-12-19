include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = htmlCheck.o

htmlCheck: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/htmlCheck $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/htmlCheck${EXE}

clean:
	rm -f $O
