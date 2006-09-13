include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = fakeLinkage.o

fakeLinkage: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/fakeLinkage $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/fakeLinkage${EXE}

clean:
	rm -f $O
