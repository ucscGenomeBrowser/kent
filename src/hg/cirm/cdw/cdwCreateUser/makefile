kentSrc = ../../../..
include ../../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} -lz
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ../lib/${MACHTYPE}/cdwlib.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


A = cdwCreateUser
O = cdwCreateUser.o 

cdwCreateUser: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}
