# Browser supports multiple trackDb's so that individual developers
# can change things rapidly without stepping on other people's toes. 
# Usually when updating it is best to update your own trackDb and
# test it to make sure it works and that you have cvs updated all
# of this dir before doing a make beta_*_update. Note that you
# must specify which trackDb you are using in your .hg.conf file
# or in the cgi-bin-$(USER)/hg.conf file. Something like: 
# db.trackDb=trackDb_YourUserName

beta_update:
	hgTrackDb zoo zooBaboon3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zoo zooCat3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zoo zooChicken3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zoo zooChimp3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zoo zooCow3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zoo zooDog3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zoo zooFugu3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zoo zooHuman3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zoo zooMouse3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zoo zooPig3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zoo zooTetra3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zoo zooZebrafish3 trackDb ../../../lib/trackDb.sql .

update:
	hgTrackDb zoo zooBaboon3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zoo zooCat3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zoo zooChicken3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zoo zooChimp3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zoo zooCow3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zoo zooDog3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zoo zooFugu3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zoo zooHuman3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zoo zooMouse3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zoo zooPig3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zoo zooRat3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zoo zooTetra3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zoo zooZebrafish3 trackDb_$(USER) ../../../lib/trackDb.sql .
