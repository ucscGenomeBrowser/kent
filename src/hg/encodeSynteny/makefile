include ../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = encodeSynteny

O = $A.o

#include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L} 
	rm -f ${HOME}/.html/syntenyComparison2.html ./$A
	mv a.out $A
	./$A > ${HOME}/.html/syntenyComparison2.html
	chmod ug+rx $A
	chmod ugo+r $A ${HOME}/.html/syntenyComparison2.html
