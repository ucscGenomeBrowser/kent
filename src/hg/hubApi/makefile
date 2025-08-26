kentSrc = ../..
include ../../inc/common.mk

cgiApi = /usr/local/apache/cgi-bin-api

L += ${HALLIBS} $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgapcgi.a $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hubApi
O = hubApi.o getData.o apiUtils.o list.o search.o findGenome.o liftOver.o

A = hubApi

include ../../inc/cgi_build_rules.mk

compile:: $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

# building your own sandbox also copies result to cgiApi directory
# if it exists

my::
	if [ -d ${cgiApi} ]; then rm -f ${cgiApi}/$A; cp -p ${CGI_BIN_USER}/$A ${cgiApi}/$A; fi

test::
	cd tests && ${MAKE} test
