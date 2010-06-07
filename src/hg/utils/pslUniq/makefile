include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = pslUniq.o

pslUniq: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/pslUniq $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/pslUniq${EXE}

compile: $O
	${CC} ${COPT} -o pslUniq $O ${MYLIBS} $L

clean:
	rm -f $O pslUniq
