include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = ultraPlateau.o

ultraPlateau: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/ultraPlateau $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/ultraPlateau

clean:
	rm -f $O
