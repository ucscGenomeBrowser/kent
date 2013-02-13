include ../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgKnownGeneList

O = $A.o

$A:	compile
	rm -f ${BINDIR}/$A${EXE}
	mv $A ${BINDIR}/$A${EXE}

mgsc: strip
	mv $A ${BINDIR}/$A

compile: $O 
	${CC} -o $A $O ${MYLIBS} ${L}

clean::
	rm -f *.o

