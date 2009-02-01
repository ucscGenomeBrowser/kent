include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bedToBigBed.o

bedToBigBed: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bedToBigBed $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bedToBigBed${EXE}

clean:
	rm -f $O
