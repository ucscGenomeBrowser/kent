include ../../inc/common.mk

#	$Id: makefile,v 1.2 2006/05/19 23:53:25 hiram Exp $

L = -lm 
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
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/${A0} ${O0} ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/${A0}${EXE}

${A1}: ${O1} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/${A1} ${O1} ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/${A1}${EXE}

${A2}: ${O2} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/${A2} ${O2} ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/${A2}${EXE}

test:
	cd tests && ./runtest

clean:
	rm -f ${O0} ${O1} ${O2} ${A0} ${A1} ${A2}

