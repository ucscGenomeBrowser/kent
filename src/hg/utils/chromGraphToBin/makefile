include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm 
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = chromGraphToBin.o

chromGraphToBin: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/chromGraphToBin $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/chromGraphToBin${EXE}

clean:
	rm -f $O
