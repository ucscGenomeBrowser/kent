JKSRCDIR = /projects/compbiousr/kent/src
JKLIBDIR = $(JKSRCDIR)/lib/$(MACHTYPE)

.c.o:
	${CC} ${COPT} ${CFLAGS} -Wimplicit -I$(JKSRCDIR)/inc -I$(JKSRCDIR)/hg/inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBS =  $(JKLIBDIR)/jkhgap.a $(JKLIBDIR)/jkweb.a

O = cgiExample.o

cgiExample: $O 
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $L
	mv a.out cgiExample
	${STRIP} cgiExample${EXE}
	chmod a+rx cgiExample
	mv cgiExample /usr/local/apache/cgi-bin

