include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../zooLib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/zoo.a

A = transitivity

O = $A.o

compile: $O 
	gcc $O $(MYLIBS) $(L) 
	mv a.out $A
	chmod a+rx $A
	mv $A $(HOME)/bin/$(MACHTYPE)/transitivity

clean::
	rm -f ${O}
