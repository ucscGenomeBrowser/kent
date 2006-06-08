include ../../inc/common.mk
HG_WARN=-Wall

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgKnownGeneList

O = $A.o

$A:	compile
	rm -f ${HOME}/bin/${MACHTYPE}/$A${EXE}
	mv $A ${HOME}/bin/${MACHTYPE}/$A${EXE}

mgsc: strip
	mv $A ${HOME}/bin/${MACHTYPE}/$A

compile: $O 
	${CC} -o $A $O ${MYLIBS} ${L}

clean::
	rm -f *.o

