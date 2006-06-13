include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm 
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = chromGraphToBin.o

chromGraphToBin: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/chromGraphToBin $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/chromGraphToBin${EXE}

clean:
	rm -f $O
