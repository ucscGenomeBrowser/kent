kentSrc = ../..
include ../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgapcgi.a $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

A = searchExample
O = searchExample.o

A = searchExample

include ../../inc/cgi_build_rules.mk

compile:: $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

