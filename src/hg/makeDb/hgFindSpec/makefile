include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgFindSpec.o

hgFindSpec: compile
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgFindSpec${EXE}

compile: $O 
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgFindSpec $O $(MYLIBS) $L

