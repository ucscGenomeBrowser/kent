include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mdToNcbiLift.o

mdToNcbiLift: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/mdToNcbiLift $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/mdToNcbiLift
