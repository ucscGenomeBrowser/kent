include ../../inc/makefile.h


L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = intronEnds.o

intronEnds: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/intronEnds $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/intronEnds
