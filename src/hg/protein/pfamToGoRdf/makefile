kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = pfamToGoRdf.o rdf.o

pfamToGoRdf: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/pfamToGoRdf $O ${MYLIBS} $L
	strip ${BINDIR}/pfamToGoRdf

clean::
	rm -f ${O}
