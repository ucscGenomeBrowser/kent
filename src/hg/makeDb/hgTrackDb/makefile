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

old_update:
	hgTrackDb hg6  trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb hg7  trackDb_$(USER) ../../lib/trackDb.sql hgRoot

update:
	hgTrackDb hg8 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb hg9 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb hg10 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb hg11 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb hg12 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb mm1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb mm2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot

beta_old_update:
	hgTrackDb hg6  trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb hg7  trackDb ../../lib/trackDb.sql hgRoot

alpha: beta_update

beta: beta_update

beta_update:
	hgTrackDb hg8 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb hg9 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb hg10 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb hg11 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb hg12 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb mm1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb mm2 trackDb ../../lib/trackDb.sql hgRoot

betaZooUpdate:
	hgTrackDb zooBaboon1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooCat1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooChicken1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooChimp1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooCow1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooDog1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooFugu1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooHuman1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooMouse1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooPig1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooRat1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooZebrafish1 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooCombo1 trackDb ../../lib/trackDb.sql hgRoot

zooUpdate:
	hgTrackDb zooBaboon1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooCat1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooChicken1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooChimp1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooCow1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooDog1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooFugu1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooHuman1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooMouse1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooPig1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooRat1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooZebrafish1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooCombo1 trackDb_$(USER) ../../lib/trackDb.sql hgRoot

betaZooUpdate2:
	hgTrackDb zooBaboon2 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooCat2 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooChicken2 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooChimp2 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooCow2 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooDog2 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooFugu2 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooHuman2 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooMouse2 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooPig2 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooRat2 trackDb ../../lib/trackDb.sql hgRoot
	hgTrackDb zooZebrafish2 trackDb ../../lib/trackDb.sql hgRoot


zooUpdate2:
	hgTrackDb zooBaboon2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooCat2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooChicken2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooChimp2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooCow2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooDog2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooFugu2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooHuman2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooMouse2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooPig2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooRat2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot
	hgTrackDb zooZebrafish2 trackDb_$(USER) ../../lib/trackDb.sql hgRoot



debug : $O
	gcc -ggdb -o hgTrackDb $O $(MYLIBS) $L	
	cp hgTrackDb $(HOME)/bin/$(MACHTYPE)/
