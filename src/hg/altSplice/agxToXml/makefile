kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = agxToXml.o

agxToXml: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/agxToXml $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/agxToXml${EXE}

test:
	agxToXml input.agx output.xml
	diff expected.xml output.xml

clean:
	rm -f $O
