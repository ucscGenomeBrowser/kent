include ../../../inc/makefile.h

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgTrackDb.o


hgTrackDb: $O 
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgTrackDb $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgTrackDb

