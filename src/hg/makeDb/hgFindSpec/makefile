include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgFindSpec.o

hgFindSpec: compile
	strip $(HOME)/bin/$(MACHTYPE)/hgFindSpec

compile: $O 
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgFindSpec $O $(MYLIBS) $L

