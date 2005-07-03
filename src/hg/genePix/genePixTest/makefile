include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = genePixTest.o

genePixTest: $O ${MYLIBS}
	${CC} ${COPT} -o genePixTest $O ${MYLIBS} $L
	${STRIP} genePixTest${EXE}

clean:
	rm -f $O
