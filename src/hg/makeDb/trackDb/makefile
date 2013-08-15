# Make your private trackDb with 
#	make update
# Your private trackDb with additional options, for example:
#       make EXTRA="-strict -settings" update
# Make it for genome-test with
#	make alpha
# Make your private trackDb on local host (if defined as localDB.XXX=XXX in .hg.conf) with
#	make updateLocal  
# Make non-private trackDb on local host (if defined as localDB.XXX=XXX in .hg.conf) with
#	make alphaLocal  
# Make for ENCODE reporting (includes release alpha and release beta)
#	make encodeReport

# Browser supports multiple trackDb's so that individual developers
# can change things rapidly without stepping on other people's toes. 
# Usually when updating it is best to update your own trackDb and
# test it to make sure it works and that you have git updated all
# of trackDb/ before doing a make alpha. Note that you
# must specify which trackDb you are using in your .hg.conf file
# or in the cgi-bin-$(USER)/hg.conf file. Something like: 
# db.trackDb=trackDb_YourUserName

# note:  new group ARCHIVED_DBS created for assemblies still needed
# to support Conservation tracks in other assemblies.  These should
# have only chromInfo table on hgwbeta and RR.  make will not rebuild
# trackDb on hgwdev unless the db is specified explicitly on the 
# command line.

include ../../../inc/common.mk

DBS = 	hg16 hg17 hg18 hg19 hg19Haps hg19Patch2 hg19Patch5 hg19Patch9 hg19Patch10 \
	venter1 \
	panPan1 \
	panTro1 panTro2 panTro3 panTro4 \
	gorGor1 gorGor2 gorGor3 \
	nomLeu1 nomLeu2 nomLeu3 \
	ponAbe2 \
	rheMac2 rheMac3 \
	papHam1 papAnu2 \
	macEug1 macEug2 \
	calJac1 calJac3 \
	otoGar1 otoGar3 tupBel1\
	saiBol1 \
	ailMel1 \
	canFam1 canFam2 canFamPoodle1 canFam3 \
	felCat3 felCat4 felCat5  \
	musFur1 \
	mm6 mm7 mm8 mm9 mm10 mm10Patch1 mm10Strains1 \
	rn3 rn4 rn5 \
	speTri1 speTri2 cavPor3 \
	chiLan1 \
	hetGla1 hetGla2 \
	sarHar1 \
	oryCun1 oryCun2 \
	bosTau2 bosTau3 bosTau4 bosTau5 bosTau6 bosTau7 \
	equCab1 \
	equCab2 \
	cerSim1 \
	oviAri1 oviAri3 \
	susScr1 susScr2 susScr3 \
	eriEur1 eriEur2 \
	sorAra1 sorAra2 \
	turTru1 turTru2 \
	vicPac1 vicPac2 \
	pteVam1 tarSyr1 \
	dipOrd1 proCap1 \
	ochPri2 ochPri3 \
	myoLuc1 myoLuc2 \
	micMur1 \
	dasNov1 dasNov2 dasNov3 \
	triMan1 \
	loxAfr1 loxAfr2 loxAfr3 \
	echTel1 echTel2 \
	monDom1 monDom4 monDom5 \
	allMis1 \
	chrPic1 \
	anoCar1 anoCar2 \
	amaVit1 geoFor1 melUnd1 \
	galGal2 galGal3 galGal4 \
	melGal1 \
	taeGut1 taeGut2 \
	latCha1 \
	xenTro1 xenTro2 xenTro3 xenTro7 \
	tetNig1 tetNig2 \
	oryLat1 oryLat2 \
	ornAna1 \
	oreNil1 oreNil2 \
	fr1 fr2 fr3 \
	petMar1 petMar2 \
	braFlo1 braFlo2 \
	cioSav1 \
	danRer3 danRer4 danRer5 danRer6 danRer7 \
	gasAcu1 \
	gadMor1 \
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
	strPur1 strPur2 strPur4 \
	nemVec1 \
	aplCal1 \
	ce2 ce3 ce4 ce5 ce6 ce7 ce8 ce9 ce10 \
	cb1 cb2 cb3 cb4 \
	caeRem1 caeRem2 caeRem3 caeRem4 \
	caeAng1 \
	caeSp71 \
	caeSp91 \
	caeSp111 \
	caeJap1 caeJap2 caeJap3 caeJap4 \
	priPac1 priPac2 \
	caePb1 caePb2 caePb3 \
	bruMal1 \
	melHap1 \
	melInc1 \
	haeCon1 \
	falciparum \
	choHof1 \
	sacCer1 sacCer2 sacCer3 \
	sc1 \
	h1n1 \
	centromers1 \
	repeats2


GSID_DBS = \
	hiv1 vax004 hivgne8 hivmn hivmn1 hivmn2 hivgne8v2 hivVax003Vax004

ARCHIVED_DBS = \
	rheMac1 \
	danRer1 \
	mm5 \
	mm6 \
	danRer2

# if trix build breaks, disable by setting to /bin/true instead of ./buildTrix
# BUILD_TRIX = /bin/true
BUILD_TRIX = ./buildTrix
HIVE_TRIX = /hive/data/inside/trix
DATA_TRIX = /data/trix
ALPHA_MACHINE = hgwdev.cse.ucsc.edu
BETA_MACHINE = qateam@hgwbeta.cse.ucsc.edu

update: #clean
	./loadTracks ${EXTRA} trackDb_${USER} hgFindSpec_${USER} ${DBS}
	./checkMetaDb alpha metaDb_${USER} ${DBS}
	${MKDIR} ${CGI_BIN}-${USER}/encode
	rm -f ${CGI_BIN}-${USER}/encode/cv.ra
	cp -p cv/alpha/cv.ra  ${CGI_BIN}-${USER}/encode
#  if you want to test track search tool with your own trix file
#	rm -f ${DBS:%=${HIVE_TRIX}/%_trackDb_${USER}.{ixx,ix}}
#	HGDB_TRACKDB=trackDb_${USER} ${BUILD_TRIX} trackDb_${USER} metaDb_${USER} ${CGI_BIN}-${USER}/encode/cv.ra ${ALPHA_MACHINE} ${HIVE_TRIX} ${DBS}
# you'll also need to set browser.trixPath to point to your trix file
# see kent/src/product/ex.hg/conf for details

alpha: clean
	${GIT} pull
	./loadTracks ${EXTRA} -release=alpha trackDb hgFindSpec ${DBS}
	./checkMetaDb alpha metaDb ${DBS}
	${MKDIR} ${CGI_BIN}/encode
	rm -f ${CGI_BIN}/encode/cv.ra
	cp -p cv/alpha/cv.ra ${CGI_BIN}/encode
	rm -f ${DBS:%=${HIVE_TRIX}/%_trackDb.{ixx,ix}}
	HGDB_TRACKDB=trackDb ${BUILD_TRIX} trackDb metaDb ${CGI_BIN}/encode/cv.ra ${ALPHA_MACHINE} ${HIVE_TRIX} ${DBS}
	./makeTrixLinks trackDb ${HIVE_TRIX} ${DBS}

onbeta:	/cluster/home/${USER}/.hg.conf.beta

beta: onbeta clean 
	${GIT} pull
	# first make files for hgwdev-beta (we share the tables with hgwbeta)
	rsync -a cv/beta/cv.ra ${CGI_BIN}-beta/encode/cv.ra
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta HGDB_TRACKDB=trackDb ${BUILD_TRIX} trackDb metaDb cv/beta/cv.ra ${ALPHA_MACHINE} ${HIVE_TRIX} ${DBS}
	# now do loads on hgwbeta
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta ./loadTracks ${EXTRA} -strict -release=beta trackDb hgFindSpec ${DBS}
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta ./checkMetaDb beta metaDb ${DBS}
	rsync -a cv/beta/cv.ra ${BETA_MACHINE}:${CGI_BIN}/encode/cv.ra
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta HGDB_TRACKDB=trackDb ${BUILD_TRIX} trackDb metaDb cv/beta/cv.ra ${BETA_MACHINE} ${DATA_TRIX} ${DBS}

public: onbeta clean
	${GIT} pull
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta ./loadTracks ${EXTRA} -strict -release=public trackDb_public hgFindSpec_public ${DBS}
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta ./checkMetaDb public metaDb_public ${DBS}
	rsync -a cv/public/cv.ra ${BETA_MACHINE}:${CGI_BIN}-public/encode
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta HGDB_TRACKDB=trackDb_public ${BUILD_TRIX} trackDb_public metaDb_public cv/public/cv.ra ${BETA_MACHINE} ${DATA_TRIX} ${DBS}

# this will fail if we are not in a beta checkout:
checkbeta:
	${GIT} branch | egrep '^[*] v[0-9]+_branch' > /dev/null

# not sure if anyone actually uses the target below. It used to be "beta".
buildBeta: checkbeta clean strict

encodeReport:
	${GIT} pull
	./loadTracks ${EXTRA} trackDb_encodeReport hgFindSpec ${DBS}

listDbs:
	@echo "# databases listed in trackDb/makefile DBS variable, with table counts"
	@for D in ${DBS}; do \
	    C=`hgsql -N -e "show tables;" $${D} 2> /dev/null | wc -l`; \
	    if [ "$${C}" -gt 0 ]; then \
		echo -e "$${D}\t$${C}"; \
	    fi \
	done

# Get rid of symbolic links (created by lower-level makefiles):
clean:
	find . -type l -exec rm {} \;

