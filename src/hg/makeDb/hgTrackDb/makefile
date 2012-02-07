include ../../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgTrackDb
O = hgTrackDb.o


hgTrackDb: $O 
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/${A}${EXE}

clean:
	rm -f $O ${A}

compile:	$O
	${CC} ${COPT} ${CFLAGS} -o ${A} $O ${MYLIBS} $L
