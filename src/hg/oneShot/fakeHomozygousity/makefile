include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = fakeHomozygousity.o

fakeHomozygousity: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/fakeHomozygousity $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/fakeHomozygousity${EXE}

clean:
	rm -f $O
