kentSrc = ../../..
# Make your private trackDb with 
#	make update
# Your private trackDb with additional options, for example:
#       make EXTRA="-strict -settings" update
# Make it for genome-test with
#	make alpha
# Make for ENCODE reporting (includes release alpha and release beta)
#	make encodeReport

# DO NOT RUN MULTIPLE INSTANCES OF THIS SCRIPT 
# Because it creates temporary files in the current directory 
# before loading them into the database.

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

DBS = \
	ailMel1 \
	allMis1 \
	amaVit1 geoFor1 melUnd1 \
	ancCey1 \
	anoCar1 anoCar2 \
	anoGam1 \
	aotNan1 \
	apiMel1 \
	apiMel2	\
	aplCal1 \
	aptMan1 \
	aquChr2 \
	ascSuu1 \
	balAcu1 \
	bisBis1 \
	bosTau2 bosTau3 bosTau4 bosTau5 bosTau6 bosTau7 bosTau8 \
	braFlo1 braFlo2 \
	bruMal1 bruMal2 \
	burXyl1 \
	caeAng1 caeAng2 \
	caeJap1 caeJap2 caeJap3 caeJap4 \
	caePb1 caePb2 caePb3 \
	caeRem1 caeRem2 caeRem3 caeRem4 \
	caeSp111 \
	caeSp51 \
	caeSp71 \
	caeSp91 \
	calJac1 calJac3 \
	calMil1 \
	canFam1 canFam2 canFamPoodle1 canFam3 \
	cb1 cb2 cb3 cb4 \
	ce2 ce3 ce4 ce5 ce6 ce7 ce8 ce9 ce10 ce11 \
	centromeres1 \
	cerAty1 \
	cerSim1 \
	chiLan1o \
	chlSab2 \
	choHof1 \
	chrPic1 chrPic2 \
	ci1 ci2 ci3 cioSav2 \
	cioSav1 \
	colAng1 \
	criGri1 \
	danRer3 danRer4 danRer5 danRer6 danRer7 danRer10 danRer11 \
	dasNov1 dasNov2 dasNov3 \
	dipOrd1 proCap1 \
	dirImm1 \
	dm1 dm2 dm3 dm6 \
	dp2 dp3 \
	droAna1 droAna2 \
	droEre1 \
	droGri1 \
	droMoj1 droMoj2 \
	droPer1 \
	droSec1 \
	droSim1 droSim2 \
	droVir1 droVir2 \
	droYak1 droYak2 \
	eboVir3 eboVir2 eboVir1 bunEbo1 sudEbo1 resEbo1 taiEbo1 zaiEbo1 \
	echTel1 echTel2 \
	equCab1 \
	equCab2 \
	eriEur1 eriEur2 \
	falciparum \
	felCat3 felCat4 felCat5  felCat8 \
	fr1 fr2 fr3 \
	gadMor1 \
	galGal2 galGal3 galGal4 galGal5 \
	galVar1 \
	gasAcu1 \
	gorGor1 gorGor2 gorGor3 gorGor4 gorGor5 \
	h1n1 \
	haeCon1 haeCon2 \
	hetBac1 \
	hetGla1 hetGla2 \
	hg16 hg17 hg18 hg19 hg38 hg19Haps hg19Patch2 hg19Patch5 hg19Patch9 hg19Patch10 hg38Patch7 hg38Patch9 hg38Patch11 \
	latCha1 \
	loaLoa1 \
	loxAfr1 loxAfr2 loxAfr3 \
	macEug1 macEug2 \
	macFas5 \
	macNem1 \
	manLeu1 \
	manPen1 \
	marVir1 \
	melGal1 melGal5 \
	melHap1 \
	melInc1 melInc2 \
	micMur1 micMur2 micMur3 \
	micOch1 \
	mm6 mm7 mm8 mm9 mm10 mm10Patch1 mm10Strains1 mm10Patch4 \
	monDom1 monDom4 monDom5 \
	musFur1 \
	myoLuc1 myoLuc2 \
	nanPar1 \
	nasLar1 \
	necAme1 \
	nemVec1 \
	neoSch1 \
	nomLeu1 nomLeu2 nomLeu3 \
	ochPri2 ochPri3 \
	oncVol1 \
	orcOrc1 \
	oreNil1 oreNil2 oreNil3 \
	ornAna1 ornAna2 \
	oryCun1 oryCun2 \
	oryLat1 oryLat2 \
	otoGar1 otoGar3 \
	oviAri1 oviAri3 \
	oxyTri2 \
	panPan1 panPan2 \
	panRed1 \
	panTro1 panTro2 panTro3 panTro4 panTro5 \
	papHam1 papAnu2 papAnu3 \
	petMar1 petMar2 \
	plasFalc3D7V12 \
	poeRet1 \
	ponAbe2 \
	priExs1 \
	priPac1 priPac2 priPac3 \
	pteVam1 tarSyr1 tarSyr2 \
	repeats2 \
	rheMac2 rheMac3 rheMac7 rheMac8 \
	rhiBie1 \
	rhiRox1 \
	rn3 rn4 rn5 rn6 \
	rouAeg1 \
	sacCer1 sacCer2 sacCer3 \
	saiBol1 \
	sarHar1 \
	sc1 \
	sorAra1 sorAra2 \
	speTri1 speTri2 cavPor3 \
	strPur1 strPur2 strPur4 \
	strRat1 strRat2 \
	susScr1 susScr2 susScr3 susScr11 \
	taeGut1 taeGut2 \
	tetNig1 tetNig2 \
	thaSir1 \
	triCas2 \
	triMan1 \
	triSpi1 \
	triSui1 \
	tupBel1 \
	tupChi1 \
	turTru1 turTru2 \
	venter1 \
	vicPac1 vicPac2 \
	xenLae2 \
	xenTro1 xenTro2 xenTro3 xenTro7 xenTro9

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

update: ${DBS:%=%_update}
	${MKDIR} ${CGI_BIN}-${USER}/encode
%_update:
	./loadTracks ${EXTRA} trackDb_${USER} hgFindSpec_${USER} $*
	./checkMetaDb alpha metaDb_${USER} $*

#  if you want to test track search tool with your own trix file
#	rm -f ${DBS:%=${HIVE_TRIX}/%_trackDb_${USER}.{ixx,ix}}
#	HGDB_TRACKDB=trackDb_${USER} ${BUILD_TRIX} trackDb_${USER} metaDb_${USER} ${CGI_BIN}-${USER}/encode/cv.ra ${ALPHA_MACHINE} ${HIVE_TRIX} ${DBS}
# you'll also need to set browser.trixPath to point to your trix file
# see kent/src/product/ex.hg/conf for details

alpha: clean
	${GIT} pull
	${MKDIR} ${CGI_BIN}/encode
	${MAKE} alpha_all

alpha_all: ${DBS:%=%_alpha}

%_alpha:
	./loadTracks ${EXTRA} -release=alpha trackDb hgFindSpec $*
	./checkMetaDb alpha metaDb $*
	rm -f ${HIVE_TRIX}/$*_trackDb.{ixx,ix}}
	HGDB_TRACKDB=trackDb ${BUILD_TRIX} trackDb metaDb cv/alpha/cv.ra ${ALPHA_MACHINE} ${HIVE_TRIX} $*
	./makeTrixLinks trackDb ${HIVE_TRIX} $*

onbeta:	/cluster/home/${USER}/.hg.conf.beta

beta: onbeta clean 
	${GIT} pull
	${MAKE} beta_all

beta_all: ${DBS:%=%_beta}

%_beta:
	# now do loads on hgwbeta
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta ./loadTracks ${EXTRA} -strict -release=beta trackDb hgFindSpec $*
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta ./checkMetaDb beta metaDb $*
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta HGDB_TRACKDB=trackDb ${BUILD_TRIX} trackDb metaDb cv/beta/cv.ra ${BETA_MACHINE} ${DATA_TRIX} $*
	# now make files for hgwdev-beta (we share the tables with hgwbeta)
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta HGDB_TRACKDB=trackDb ${BUILD_TRIX} trackDb metaDb cv/beta/cv.ra ${ALPHA_MACHINE} ${HIVE_TRIX} $*

public: onbeta clean
	${GIT} pull
	${MAKE} public_all

public_all: ${DBS:%=%_public}

%_public:
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta ./loadTracks ${EXTRA} -strict -release=public trackDb_public hgFindSpec_public $*
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta ./checkMetaDb public metaDb_public $*
	HGDB_CONF=/cluster/home/${USER}/.hg.conf.beta HGDB_TRACKDB=trackDb_public ${BUILD_TRIX} trackDb_public metaDb_public cv/public/cv.ra ${BETA_MACHINE} ${DATA_TRIX} $*

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

