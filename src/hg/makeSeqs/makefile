JKSRCDIR = /cluster/home/ctrople/kent/src
JKLIBDIR = $(JKSRCDIR)/lib/$(MACHTYPE)

.c.o:
	gcc -ggdb -Wimplicit -I$(JKSRCDIR)/inc -I$(JKSRCDIR)/hg/inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBS =  $(JKLIBDIR)/jkhgap.a $(JKLIBDIR)/jkweb.a

O = makeSeqs.o

makeSeqs: $O 
	gcc -ggdb $O $(MYLIBS) $(L) 
	mv a.out makeSeqs
	strip makeSeqs
	chmod a+rx makeSeqs
#	cp makeSeqs $(HOME)/bin/$(MACHTYPE)
