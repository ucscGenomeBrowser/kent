include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = pfamXref
O = $A.o

$A: $O ${MYLIBS}
	gcc -ggdb -o ${HOME}/bin/${MACHTYPE}/$A $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/$A

clean::
	rm -f $O
