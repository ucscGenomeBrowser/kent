include ../../inc/makefile.h


L = -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraHubStop.o

paraHubStop: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/paraHubStop $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip $(HOME)/bin/$(MACHTYPE)/paraHubStop
	chmod 744 $(HOME)/bin/$(MACHTYPE)/paraHubStop
