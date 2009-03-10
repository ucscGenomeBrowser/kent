include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = visiGeneSearch.o

visiGeneSearch: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/visiGeneSearch $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/visiGeneSearch${EXE}

clean:
	rm -f $O
