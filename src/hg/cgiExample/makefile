kentSrc = ../..
include ../../inc/common.mk

JKSRCDIR = /projects/compbiousr/kent/src
JKLIBDIR = $(JKSRCDIR)/lib/$(MACHTYPE)

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} 
MYLIBS =  $(JKLIBDIR)/jkhgap.a $(JKLIBDIR)/jkweb.a

O = cgiExample.o

cgiExample: $O 
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $L
	mv ${AOUT} cgiExample
	${STRIP} cgiExample${EXE}
	chmod a+rx cgiExample
	mv cgiExample /usr/local/apache/cgi-bin

clean::
	rm -f ${O}
