kentSrc = ../..
include ../../inc/common.mk

L += ${HALLIBS} $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hubApi
O = hubApi.o getData.o apiUtils.o

A = hubApi

include ../../inc/cgi_build_rules.mk

compile:: $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

test::
	cd tests && make test
