include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = liftAcross.o

liftAcross: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/liftAcross $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/liftAcross${EXE}

compile:	${O}
	${CC} ${COPT} -o liftAcross $O ${MYLIBS} $L

clean:
	rm -f $O liftAcross
