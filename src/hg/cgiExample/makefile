JKSRCDIR = /projects/compbiousr/kent/src
JKLIBDIR = $(JKSRCDIR)/lib/$(MACHTYPE)

.c.o:
	gcc -ggdb -Wimplicit -I$(JKSRCDIR)/inc -I$(JKSRCDIR)/hg/inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBS =  $(JKLIBDIR)/jkhgap.a $(JKLIBDIR)/jkweb.a

O = cgiExample.o

cgiExample: $O 
	gcc $O $(MYLIBS) $L
	mv a.out cgiExample
	strip cgiExample
	chmod a+rx cgiExample
	mv cgiExample /usr/local/apache/cgi-bin

