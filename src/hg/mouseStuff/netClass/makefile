include ../../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = netClass.o

netClass: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/netClass $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/netClass${EXE}

compile:: $O
	${CC} ${COPT} ${CFLAGS} -o netClass${EXE} $O ${MYLIBS} $L

clean::
	rm -f $O netClass
