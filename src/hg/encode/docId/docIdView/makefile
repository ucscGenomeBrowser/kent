include ../../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/docId.a $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a 

A = docIdView
O = docIdView.o

include ../../../../inc/cgi_build_rules.mk

compile:: $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
