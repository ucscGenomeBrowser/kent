# Browser supports multiple trackDb's so that individual developers
# can change things rapidly without stepping on other people's toes. 
# Usually when updating it is best to update your own trackDb and
# test it to make sure it works and that you have cvs updated all
# of this dir before doing a make beta_*_update. Note that you
# must specify which trackDb you are using in your .hg.conf file
# or in the cgi-bin-$(USER)/hg.conf file. Something like: 
# db.trackDb=trackDb_YourUserName

beta_update:
	hgTrackDb zooBaboon3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zooCat3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zooChicken3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zooChimp3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zooCow3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zooDog3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zooFugu3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zooHuman3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zooMouse3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zooPig3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zooTetra3 trackDb ../../../lib/trackDb.sql .
	hgTrackDb zooZebrafish3 trackDb ../../../lib/trackDb.sql .

update:
	hgTrackDb zooBaboon3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zooCat3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zooChicken3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zooChimp3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zooCow3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zooDog3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zooFugu3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zooHuman3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zooMouse3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zooPig3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zooRat3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zooTetra3 trackDb_$(USER) ../../../lib/trackDb.sql .
	hgTrackDb zooZebrafish3 trackDb_$(USER) ../../../lib/trackDb.sql .
