include ../../../inc/makefile.h

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../zooLib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/zoo.a

A = alignToPsl

O = $A.o

compile: $O 
	gcc $O $(MYLIBS) $(L) 
	mv a.out $A
	chmod a+rx $A
	mv $A $(HOME)/bin/$(MACHTYPE)/alignToPsl

