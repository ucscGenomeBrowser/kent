include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = splatCheck2.o

splatCheck2: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/splatCheck2 $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/splatCheck2${EXE}

clean:
	rm -f $O
