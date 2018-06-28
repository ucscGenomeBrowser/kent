kentSrc = ../..
include ../../inc/common.mk


L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = splignToPsl

O = $A.o

include ../../inc/cgi_build_rules.mk

mgsc: compile
	cp $A ${CGI_BIN}-${USER}/$A

test: compile
	cp $A ${CGI_BIN}-${USER}/$A
compile: $O 
	${CC} $O ${F64} ${MYLIBS} ${L} #/cse/grads/markd/lib/libefence.a -lpthread 
	#${CC} $O ${F64} ${MYLIBS} ${L}	
	mv ${AOUT} $A${EXE}
	chmod a+rx $A
