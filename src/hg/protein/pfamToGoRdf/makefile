include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = pfamToGoRdf.o rdf.o

pfamToGoRdf: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pfamToGoRdf $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/pfamToGoRdf

clean:
	rm -f $O
