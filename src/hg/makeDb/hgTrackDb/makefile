.c.o:
	gcc -ggdb -Wformat -Wimplicit -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgTrackDb.o



hgTrackDb: $O 
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgTrackDb $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgTrackDb


# Browser supports multiple trackDb's so that individual developers
# can change things rapidly without stepping on other people's toes. 
# Usually when updating it is best to update your own trackDb and
# test it to make sure it works and that you have cvs updated all
# of hgRoot before doing a make beta_*_update. Note that you
# must specify which trackDb you are using in your .hg.conf file
# or in the cgi-bin-$(USER)/hg.conf file. Something like: 
# db.trackDb=trackDb_YourUserName

update:
	hgTrackDb human hg8 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb human hg9 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb human hg10 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb human hg11 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb human hg12 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb human hg13 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb mouse mm1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb mouse mm2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb mouse rn1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb rat rn1 trackDb ../../lib/trackDb.sql hgRoot
	cd zoo; make update; cd ..

old_update:
	hgTrackDb human hg6 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb human hg7 trackDb_$(USER) ../../lib/trackDb.sql hgRoot

alpha:
	hgTrackDb human hg8 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb human hg9 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb human hg10 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb human hg11 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb human hg12 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb human hg13 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb mouse mm1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb mouse mm2 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb rat rn1 trackDb ../../lib/trackDb.sql hgRoot
	cd zoo; make alpha; cd ..

old_alpha:
	hgTrackDb human hg6  trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb human hg7  trackDb ../../lib/trackDb.sql hgRoot


debug : $O
	gcc -ggdb -o hgTrackDb $O $(MYLIBS) $L	
	cp hgTrackDb $(HOME)/bin/$(MACHTYPE)/
