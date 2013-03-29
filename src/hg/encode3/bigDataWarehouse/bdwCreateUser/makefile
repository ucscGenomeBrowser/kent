include ../../../../inc/common.mk

L += $(MYSQLLIBS) -lm -lz
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ../lib/${MACHTYPE}/bdwlib.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


A = bdwCreateUser
O = bdwCreateUser.o 

bdwCreateUser: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}
