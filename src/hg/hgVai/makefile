kentSrc = ../..
include ../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgapcgi.a $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgVai
O = hgVai.o libifyMe.o

include ../../inc/cgi_build_rules.mk

compile:: $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

