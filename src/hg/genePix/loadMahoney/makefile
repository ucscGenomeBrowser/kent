include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = loadMahoney.o mahoney.o

loadMahoney: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/loadMahoney $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/loadMahoney${EXE}

clean:
	rm -f $O
