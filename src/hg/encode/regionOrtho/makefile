include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = regionOrtho
O = $A.o

#include ../inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L} 
	mv -f a.out ${HOME}/bin/${MACHTYPE}/${A}
	rm -f ${HOME}/.html/${A}.html
	$A > ${HOME}/.html/${A}.html
	chmod ugo+r ${HOME}/.html/${A}.html
