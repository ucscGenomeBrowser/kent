include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = hgDropSplitTable.o

hgDropSplitTable: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgDropSplitTable $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/hgDropSplitTable${EXE}

clean:
	rm -f $O
