include ../../../inc/common.mk


L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = pbGateway

O = $A.o

include ../../../inc/cgi_build_rules.mk

lib::
	cd ../../../lib; make

compile: $O 
	${CC} -o $A $O ${MYLIBS} ${L}
	${STRIP} $A
	chmod a+rx $A

