include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = fastqToFa.o

fastqToFa: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/fastqToFa $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/fastqToFa${EXE}

clean:
	rm -f $O
