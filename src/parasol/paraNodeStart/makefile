include ../../inc/makefile.h


L = -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraNodeStart.o

paraNodeStart: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/paraNodeStart $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip $(HOME)/bin/$(MACHTYPE)/paraNodeStart
	chmod 744 $(HOME)/bin/$(MACHTYPE)/paraNodeStart
