# Make your private trackDb with 
#	make update
# Make it for genome-test with
#	make alpha
# Make your private trackDb on local host (if defined as localDB.XXX=XXX in .hg.conf) with
#	make updateLocal  
# Make non-private trackDb on local host (if defined as localDB.XXX=XXX in .hg.conf) with
#	make alphaLocal  

# Browser supports multiple trackDb's so that individual developers
# can change things rapidly without stepping on other people's toes. 
# Usually when updating it is best to update your own trackDb and
# test it to make sure it works and that you have cvs updated all
# of trackDb/ before doing a make alpha. Note that you
# must specify which trackDb you are using in your .hg.conf file
# or in the cgi-bin-$(USER)/hg.conf file. Something like: 
# db.trackDb=trackDb_YourUserName

# FIXME: the execution of commands with $$cmd broken on the mgc
# machine and required using eval $$cmd; I am not sure why this
# is working other places. 

# note:  new group ARCHIVED_DBS created for assemblies still needed
# to support Conservation tracks in other assemblies.  These should
# have only chromInfo table on hgwbeta and RR.  make will not rebuild
# trackDb on hgwdev unless the db is specified explicitly on the 
# command line.


include ../../../inc/common.mk

DBS = 	hg16 hg17 hg18\
	venter1 \
	panTro1 panTro2 \
	gorGor1 \
	ponAbe1 ponAbe2 \
	rheMac2 \
	calJac1 \
	otoGar1 tupBel1\
	canFam1 canFam2 \
	felCat3 \
	mm6 mm7 mm8 mm9 \
	rn3 rn4 rn5 \
	speTri1 cavPor3 \
	oryCun1 \
	bosTau2 bosTau3 bosTau4 \
	equCab1 \
	equCab2 \
	eriEur1 sorAra1 turTru1 vicVic1 pteVam1 tarSyr1 \
	dipOrd1 proCap1 ochPri2 myoLuc1 \
	micMur1 \
	dasNov1 dasNov2 \
	loxAfr1 loxAfr2 \
	echTel1 \
	monDom1 monDom4 monDom5 \
	anoCar1 \
	galGal2 galGal3 \
	taeGut1 \
	xenTro1 xenTro2 \
	tetNig1 \
	oryLat1 oryLat2 \
	ornAna1 \
	fr1 fr2 \
	petMar1 \
	braFlo1 \
	cioSav1 \
	danRer3 danRer4 danRer5 \
	gasAcu1 \
	dm1 dm2 dm3 \
	dp2 dp3 \
	droYak1 droYak2 \
	droAna1 droAna2 \
	droMoj1 droMoj2 \
	droVir1 droVir2 \
	droEre1 \
	droSim1 \
	droGri1 \
	droPer1 \
	droSec1 \
	anoGam1 \
	apiMel1 \
	apiMel2	\
	triCas2 \
	ci1 ci2 cioSav2 \
	strPur1 strPur2 \
    nemVec1 \
	ce2 ce3 ce4 ce6 \
	cb1 cb2 cb3 \
	caeRem1 caeRem2 caeRem3 \
	caeJap1 \
	priPac1 \
	caePb1 caePb2 \
	falciparum \
	choHof1 \
	sacCer1 \
	sc1 \
	gliRes13 homIni13 homIni14 priMat13 canHg12 borEut13 \
	rodEnt13 euaGli13 catArr1 homPan20 homIni20

MGC_DBS = \
	hg16 hg17 hg18\
	mm5 mm6 mm7 mm8 \
	rn3 rn4 \
	danRer2 danRer3 danRer4 danRer5 \
	xenTro1 xenTro2 \
	bosTau2

GSID_DBS = \
	hiv1 vax004 hivgne8 hivmn hivmn1 hivmn2 hivgne8v2 hivVax003Vax004

ARCHIVED_DBS = \
	rheMac1 \
	danRer1 \
	mm5 \
	mm6 \
	danRer2

update: #clean
	./loadTracks trackDb_${USER} hgFindSpec_${USER} ${DBS}

alpha: clean
	${CVS} -q up -d -P .
	./loadTracks -release=alpha trackDb hgFindSpec ${DBS}

strict: clean
	${CVS} -q up -d -P .
	./loadTracks -strict -release=beta trackDb hgFindSpec ${DBS}

# this will fail if we are not in a beta checkout:
checkbeta:
	${CVS} status trackDb.ra | egrep 'v[0-9]+_branch' > /dev/null

beta: checkbeta clean strict

# This target adds trackDbMgc to the mgc-specific server.  Also
# loads the grpMgc table.
mgc:
	ln -sf  human/hg17/exoniphy.html exoniphy.html
	ln -sf  human/hg17/exoniphyGene.html exoniphyGene.html
	ln -sf  mgcTwinscan.html mgcTwinscanEst.html
	ln -sf  mgcNScan.html mgcNScanEst.html
	./loadTracks -strict -release=beta -raName=trackDbMgc.ra -grpSql=grpMgc.sql trackDbMgc hgFindSpecMgc ${MGC_DBS}

# Get rid of symbolic links (created by lower-level makefiles):
clean:
	find . -type l -exec rm {} \;

