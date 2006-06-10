include ../../inc/common.mk

HG_WARN=-Wall

L = -lm ${MYSQLLIBS} 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgc

O = $A.o expClick.o mafClick.o regMotif.o wiggleClick.o rnaFoldClick.o transMapClick.o

include ../../inc/cgi_build_rules.mk

mgsc: compile
	mv $A ${CGI_BIN}-mgsc/$A

compile: $O 
	${CC} $O ${F64} ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
