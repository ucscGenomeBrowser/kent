include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 


O = txCdsGoodBed.o

txCdsGoodBed: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txCdsGoodBed $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txCdsGoodBed${EXE}

clean:
	rm -f $O
