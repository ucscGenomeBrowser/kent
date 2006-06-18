include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = hgRenameSplitTable.o

hgRenameSplitTable: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgRenameSplitTable $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/hgRenameSplitTable${EXE}

clean:
	rm -f $O
