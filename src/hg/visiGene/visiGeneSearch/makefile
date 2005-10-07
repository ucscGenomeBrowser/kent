include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = visiGeneSearch.o

visiGeneSearch: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/visiGeneSearch $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/visiGeneSearch

clean:
	rm -f $O
