include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = b2bb.o

b2bb: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/b2bb $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/b2bb${EXE}

clean:
	rm -f $O
