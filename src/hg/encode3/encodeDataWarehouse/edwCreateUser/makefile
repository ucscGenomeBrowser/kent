kentSrc = ../../../..
include ../../../../inc/common.mk

L += $(MYSQLLIBS) -lm -lz
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ../lib/${MACHTYPE}/edwlib.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


A = edwCreateUser
O = edwCreateUser.o 

edwCreateUser: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}
