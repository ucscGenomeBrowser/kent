include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS = ../lib/${MACHTYPE}/encode.a ${MYLIBDIR}/jkweb.a 

A = encodePatchTdb
O = encodePatchTdb.o

encodePatchTdb: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A} ${O}

test: 
	cd tests; test.sh
