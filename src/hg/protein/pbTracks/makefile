include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm 
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkpbap.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = pbTracks

O = $A.o

include ../../../inc/cgi_build_rules.mk

lib::
	cd ../../../lib; make

compile: $O 
	${CC} -o $A $O ${MYLIBS} ${L}
	${STRIP} $A
	chmod a+rx $A

mgsc: compile
	rm -f ${CGI_BIN}-mgsc/$A
	mv $A ${CGI_BIN}-mgsc/$A

