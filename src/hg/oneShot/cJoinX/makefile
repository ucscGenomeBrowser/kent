include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = cJoinX.o

cJoinX: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/cJoinX $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/cJoinX${EXE}

clean:
	rm -f $O
