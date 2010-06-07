include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm 
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = chromGraphFromBin.o

chromGraphFromBin: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/chromGraphFromBin $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/chromGraphFromBin${EXE}

clean:
	rm -f $O
