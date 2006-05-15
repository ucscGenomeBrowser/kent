include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgLoadBed.o
A = hgLoadBed

hgLoadBed: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/${A} $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/hgLoadBed${EXE}

compile:	${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${A} $O ${MYLIBS} $L

clean::
	rm -f ${O} ${A} tests/bed.tab

test:
	cd tests && ./RunTest.sh

test_verbose:
	cd tests && ./RunTest.sh -verbose

