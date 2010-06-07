include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = hgRenameSplitTable.o

hgRenameSplitTable: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgRenameSplitTable $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgRenameSplitTable${EXE}

clean:
	rm -f $O
