include ../../inc/makefile.h


L = 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = splitFa.o 

splitFa: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/splitFa $O ../../lib/$(MACHTYPE)/jkweb.a

