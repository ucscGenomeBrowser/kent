include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = ixIxx.o

ixIxx: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/ixIxx $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/ixIxx${EXE}

clean:
	rm -f $O
