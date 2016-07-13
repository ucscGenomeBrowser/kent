kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txGeneBakeOff.o refCluster.o

txGeneBakeOff: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txGeneBakeOff $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txGeneBakeOff${EXE}

clean:
	rm -f $O
