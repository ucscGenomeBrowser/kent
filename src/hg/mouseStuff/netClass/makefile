include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = netClass.o

netClass: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/netClass $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/netClass${EXE}

compile:: $O
	${CC} ${COPT} ${CFLAGS} -o netClass${EXE} $O ${MYLIBS} $L

clean::
	rm -f $O netClass
