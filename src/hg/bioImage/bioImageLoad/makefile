include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = bioImageLoad.o

bioImageLoad: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/bioImageLoad $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/bioImageLoad${EXE}

clean:
	rm -f $O
