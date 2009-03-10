include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsRefBestEvOnly.o

txCdsRefBestEvOnly: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsRefBestEvOnly $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsRefBestEvOnly${EXE}

clean:
	rm -f $O
