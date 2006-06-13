include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm 
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = chromGraphFromBin.o

chromGraphFromBin: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/chromGraphFromBin $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/chromGraphFromBin${EXE}

clean:
	rm -f $O
