# Make your private trackDb with 
#	make update
# Make it for genome-test with
#	make alpha

# Browser supports multiple trackDb's so that individual developers
# can change things rapidly without stepping on other people's toes. 
# Usually when updating it is best to update your own trackDb and
# test it to make sure it works and that you have cvs updated all
# of hgRoot before doing a make beta_*_update. Note that you
# must specify which trackDb you are using in your .hg.conf file
# or in the cgi-bin-$(USER)/hg.conf file. Something like: 
# db.trackDb=trackDb_YourUserName

update:
	hgTrackDb human hg10 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg11 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg12 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg13 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg14 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg15 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb mouse mm1 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb mouse mm2 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb mouse mm3 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb rat rn1 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb rat rn2 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb sars sc1 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb cIntestinalis ci1 trackDb_$(USER) ../../lib/trackDb.sql .
	cd zoo; make update; cd ..

old_update:
	hgTrackDb human hg6 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg7 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg8 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg9 trackDb_$(USER) ../../lib/trackDb.sql .

alpha:
	cvs up -d -P .
	hgTrackDb human hg10 trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg11 trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg12 trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg13 trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg14 trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg15 trackDb ../../lib/trackDb.sql .
	hgTrackDb mouse mm1 trackDb ../../lib/trackDb.sql .
	hgTrackDb mouse mm2 trackDb ../../lib/trackDb.sql .
	hgTrackDb mouse mm3 trackDb ../../lib/trackDb.sql .
	hgTrackDb rat rn1 trackDb ../../lib/trackDb.sql .
	hgTrackDb rat rn2 trackDb ../../lib/trackDb.sql .
	hgTrackDb sars sc1 trackDb ../../lib/trackDb.sql .
	cd zoo; make alpha; cd ..

beta:
	# this will fail if we are not in a beta checkout:
	cvs status trackDb.ra | egrep 'v[0-9]+_branch' > /dev/null
	cvs up -d -P .
	hgTrackDb human hg10 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb human hg11 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb human hg12 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb human hg13 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb human hg14 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb human hg15 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb mouse mm1 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb mouse mm2 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb mouse mm3 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb rat rn1 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb rat rn2 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb sars sc1 trackDb_beta ../../lib/trackDb.sql .
	cd zoo; make beta; cd ..

old_alpha:
	hgTrackDb human hg6  trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg7  trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg8  trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg9  trackDb ../../lib/trackDb.sql .

# This target initializes the MGC-browser database, overriding the default 
# visibility settings
mgc:
	hgTrackDb -visibility=mgcView.ra human hg13 trackDb ../../lib/trackDb.sql .
	hgTrackDb -visibility=mgcView.ra mouse mm3 trackDb ../../lib/trackDb.sql .
	hgTrackDb -visibility=mgcView.ra rat rn1 trackDb ../../lib/trackDb.sql .

