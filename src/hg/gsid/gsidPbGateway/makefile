include ../../../inc/common.mk


L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = gsidPbGateway

O = $A.o

include ../../../inc/cgi_build_rules.mk

lib::
	cd ../../../lib && ${MAKE}

compile: $O 
	${CC} -o $A $O ${MYLIBS} ${L}

mgsc: strip
	mv $A ${CGI_BIN}/$A

