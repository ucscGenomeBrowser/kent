include ../../inc/common.mk

#	$Id: makefile,v 1.4 2009/03/10 00:34:50 galt Exp $

L += -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

all::	gmtime localtime mktime

O0 = gmtime.o
O1 = localtime.o
O2 = mktime.o

A0 = gmtime
A1 = localtime
A2 = mktime

${A0}: ${O0} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A0} ${O0} ${MYLIBS} $L
	${STRIP} ${BINDIR}/${A0}${EXE}

${A1}: ${O1} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A1} ${O1} ${MYLIBS} $L
	${STRIP} ${BINDIR}/${A1}${EXE}

${A2}: ${O2} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A2} ${O2} ${MYLIBS} $L
	${STRIP} ${BINDIR}/${A2}${EXE}

test:
	cd tests && ./runtest

clean:
	rm -f ${O0} ${O1} ${O2} ${A0} ${A1} ${A2}

