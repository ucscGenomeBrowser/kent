.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgTrackDb.o

hgTrackDb: $O 
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgTrackDb $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgTrackDb

old_update:
	hgTrackDb hg6 ../../lib/trackDb.sql hgRoot
	hgTrackDb hg7 ../../lib/trackDb.sql hgRoot

update:
	hgTrackDb hg8 ../../lib/trackDb.sql hgRoot
	hgTrackDb hg9 ../../lib/trackDb.sql hgRoot
	hgTrackDb hg10 ../../lib/trackDb.sql hgRoot
	hgTrackDb hg11 ../../lib/trackDb.sql hgRoot
	hgTrackDb mm1 ../../lib/trackDb.sql hgRoot
	hgTrackDb mm2 ../../lib/trackDb.sql hgRoot

zooMouse1:
	hgTrackDb zooMouse1 ../../lib/trackDb.sql hgRoot
