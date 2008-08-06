include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = kgBioCyc1.o

kgBioCyc1: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/kgBioCyc1 $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/kgBioCyc1${EXE}

clean:
	rm -f $O
