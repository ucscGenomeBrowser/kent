JKSRCDIR = /cluster/home/ctrople/kent/src
JKLIBDIR = $(JKSRCDIR)/lib/$(MACHTYPE)

.c.o:
	gcc -ggdb -Wimplicit -I$(JKSRCDIR)/inc -I$(JKSRCDIR)/hg/inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBS =  $(JKLIBDIR)/jkhgap.a $(JKLIBDIR)/jkweb.a

O = gpcrParser.o

gpcrParser: $O 
	gcc -ggdb $O $(MYLIBS) $(L) 
	mv a.out gpcrParser
#	strip gpcrParser
	chmod a+rx gpcrParser

