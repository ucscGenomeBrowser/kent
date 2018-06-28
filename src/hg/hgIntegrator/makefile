kentSrc = ../..
include ../../inc/common.mk

#L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a ../../htslib/libhts.a $(MYSQLLIBS)

A = hgIntegrator
O = ${A}.o

include ../../inc/cgi_build_rules.mk

compile:: $O
	${CC} $O ${MYLIBS} #${L}
	mv ${AOUT} $A${EXE}

