include ../../inc/common.mk


L = 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooLiftSpec.o

ooLiftSpec: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -o $(HOME)/bin/$(MACHTYPE)/ooLiftSpec $O $(MYLIBS) $L

