include ../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS = $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

#JKSRCDIR = $(HOME)/kent/src
#JKLIBDIR = $(JKSRCDIR)/lib/$(MACHTYPE)
# MYLIBS =  $(JKLIBDIR)/jkhgap.a $(JKLIBDIR)/jkweb.a

#.c.o:
#	gcc -ggdb -Wimplicit -I$(JKSRCDIR)/inc -I$(JKSRCDIR)/hg/inc -c $*.c


O = gpcrParser.o

gpcrParser: $O 
	gcc -ggdb $O $(MYLIBS) $(L) 
	mv a.out gpcrParser
	chmod a+rx gpcrParser
	cp gpcrParser $(HOME)/bin/$(MACHTYPE)
