include ../../../inc/makefile.h

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = orthologBySynteny.o

orthologBySynteny: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/orthologBySynteny $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/orthologBySynteny
