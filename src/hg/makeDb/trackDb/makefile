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
	hgTrackDb human hg13 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg15 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg16 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb malaria falciparum trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb mouse mm2 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb mouse mm3 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb -visibility=mouse/mm4/visibility.ra mouse mm4 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb rat rn2 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb -visibility=rat/visibility.ra rat rn3 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb sars sc1 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb squirt ci1 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb -visibility=fugu/visibility.ra fugu fr1 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb worm ce1 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb worm cb1 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb drosophila dm1 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb drosophila dp1 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb sacCer sacCer1 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb chicken galGal1 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb chicken galGal2 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb archae pyrAer1 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb archae pyrFur2 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb chimp panTro1 trackDb_$(USER) ../../lib/trackDb.sql .
	cd zoo; make update; cd ..

old_update:
	hgTrackDb human hg6 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg7 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg8 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg9 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg10 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb human hg11 trackDb_$(USER) ../../lib/trackDb.sql .
	hgTrackDb mouse mm1 trackDb_$(USER) ../../lib/trackDb.sql .

alpha:
	cvs up -d -P .
	hgTrackDb human hg13 trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg15 trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg16 trackDb ../../lib/trackDb.sql .
	hgTrackDb mouse mm2 trackDb ../../lib/trackDb.sql .
	hgTrackDb mouse mm3 trackDb ../../lib/trackDb.sql .
	hgTrackDb -visibility=mouse/mm4/visibility.ra mouse mm4 trackDb ../../lib/trackDb.sql .
	hgTrackDb rat rn2 trackDb ../../lib/trackDb.sql .
	hgTrackDb -visibility=rat/visibility.ra rat rn3 trackDb ../../lib/trackDb.sql .
	hgTrackDb sars sc1 trackDb ../../lib/trackDb.sql .
	hgTrackDb squirt ci1 trackDb ../../lib/trackDb.sql .
	hgTrackDb fugu fr1 trackDb ../../lib/trackDb.sql .
	hgTrackDb -visibility=fugu/visibility.ra fugu fr1 trackDb ../../lib/trackDb.sql .
	hgTrackDb worm ce1 trackDb ../../lib/trackDb.sql .
	hgTrackDb worm cb1 trackDb ../../lib/trackDb.sql .
	hgTrackDb drosophila dm1 trackDb ../../lib/trackDb.sql .
	hgTrackDb drosophila dp1 trackDb ../../lib/trackDb.sql .
	hgTrackDb sacCer sacCer1 trackDb ../../lib/trackDb.sql .
	hgTrackDb chicken galGal1 trackDb ../../lib/trackDb.sql .
	hgTrackDb chicken galGal2 trackDb ../../lib/trackDb.sql .
	hgTrackDb chimp panTro1 trackDb ../../lib/trackDb.sql .
	cd zoo; make alpha; cd ..
	cp human/hg13/description.html /gbdb/hg13/html
	cp human/hg15/description.html /gbdb/hg15/html
	cp human/hg16/description.html /gbdb/hg16/html
	cp mouse/mm2/description.html /gbdb/mm2/html
	cp mouse/mm3/description.html /gbdb/mm3/html
	cp mouse/mm4/description.html /gbdb/mm4/html
	cp rat/rn2/description.html /gbdb/rn2/html
	cp rat/rn3/description.html /gbdb/rn3/html
	cp fugu/fr1/description.html /gbdb/fr1/html
	cp drosophila/dm1/description.html /gbdb/dm1/html
	cp worm/ce1/description.html /gbdb/ce1/html
	cp worm/cb1/description.html /gbdb/cb1/html
	cp sars/sc1/description.html /gbdb/sc1/html
	cp zoo/description.html /gbdb/zoo3/html
	cp sacCer/sacCer1/description.html /gbdb/sacCer1/html
	cp chicken/galGal1/description.html /gbdb/galGal1/html
	cp chicken/galGal2/description.html /gbdb/galGal2/html
	cp archae/pyrAer1/description.html /gbdb/pyrAer1/html
	cp chimp/panTro1/description.html /gbdb/panTro1/html

strict:
	hgTrackDb -strict=yes human hg13 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes human hg15 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes human hg16 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes mouse mm2 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes mouse mm3 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes mouse mm4 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes rat rn2 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes -visibility=rat/visibility.ra rat rn3 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes sars sc1 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes -visibility=fugu/visibility.ra fugu fr1 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes worm ce1 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes worm cb1 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes drosophila dm1 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes drosophila dp1 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes sacCer sacCer1 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes chicken galGal1 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes chicken galGal2 trackDb ../../lib/trackDb.sql .
	hgTrackDb -strict=yes chimp panTro1 trackDb ../../lib/trackDb.sql .

beta:
	# this will fail if we are not in a beta checkout:
	cvs status trackDb.ra | egrep 'v[0-9]+_branch' > /dev/null
	cvs up -d -P .
	hgTrackDb human hg13 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb human hg15 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb human hg16 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb mouse mm2 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb mouse mm3 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb mouse mm4 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb rat rn2 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb -visibility=rat/visibility.ra rat rn3 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb sars sc1 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb squirt ci1 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb -visibility=fugu/visibility.ra fugu fr1 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb worm ce1 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb worm cb1 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb drosophila dm1 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb drosophila dp1 trackDb_beta ../../lib/trackDb.sql .
	hgTrackDb sacCer sacCer1 trackDb ../../lib/trackDb.sql .
	hgTrackDb chicken galGal1 trackDb ../../lib/trackDb.sql .
	hgTrackDb chicken galGal2 trackDb ../../lib/trackDb.sql .
	hgTrackDb chimp panTro1 trackDb ../../lib/trackDb.sql .
	cd zoo; make beta; cd ..

old_alpha:
	hgTrackDb human hg6  trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg7  trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg8  trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg9  trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg10  trackDb ../../lib/trackDb.sql .
	hgTrackDb human hg11  trackDb ../../lib/trackDb.sql .

# This target adds trackDbLocal to for the mgc-specific server.  This
# 
mgc:
	hgTrackDb -raName=trackDbMgc.ra human hg15 trackDbLocal ../../lib/trackDb.sql .
	hgTrackDb -raName=trackDbMgc.ra human hg16 trackDbLocal ../../lib/trackDb.sql .
	hgTrackDb -raName=trackDbMgc.ra human mm3 trackDbLocal ../../lib/trackDb.sql .
	hgTrackDb -raName=trackDbMgc.ra human mm4 trackDbLocal ../../lib/trackDb.sql .
