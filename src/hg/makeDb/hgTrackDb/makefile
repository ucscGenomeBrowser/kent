.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgTrackDb.o

hgTrackDb: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgTrackDb $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgTrackDb

update:
	hgTrackDb hg5 ../../lib/trackDb.sql hgRoot
	hgTrackDb hg6 ../../lib/trackDb.sql hgRoot
	hgTrackDb hg7 ../../lib/trackDb.sql hgRoot
