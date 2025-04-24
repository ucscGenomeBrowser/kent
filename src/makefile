include inc/localEnvironment.mk
include inc/common.mk

all: utils cgi blatSuite

alpha: check-alpha clean
	${MAKE} utils-alpha cgi-alpha blatSuite

beta: check-beta clean cgi-beta

# do a git update and clean
update:
	git pull
	${MAKE} clean
##
# topLibs
##
TOP_LIBS = lib parasol/lib htslib
ifneq ($(wildcard jkOwnLib/*),)
  TOP_LIBS += jkOwnLib
endif
topLibs: topChecks | ${TOP_LIBS:%=%.topLibs}
%.topLibs:
	${MAKE} -C $*

topChecks:
	@./checkUmask.sh
	@MACHTYPE=${MACHTYPE} ./machTest.sh

optLib:
	cd optimalLeaf && ${MAKE}

##
# hgLib
##
ifneq ($(wildcard hg/makefile),)
   HGLIB_DIRS += hg/lib
   HGLIB_CHECK = hgLibChecks
endif
ifneq ($(wildcard hg/cgilib/makefile),)
   HGLIB_DIRS += hg/cgilib
endif

hgLib: ${HGLIB_CHECK} | ${HGLIB_DIRS:%=%.hgLib}
%.hgLib:
	${MAKE} -C $*
hgLibChecks:
	@./hg/sqlEnvTest.sh

libs: topLibs hgLib optLib

cgi: libs
	cd hg && ${MAKE} cgi
	cd utils/bedToBigBed && ${MAKE} cgi

cgi-alpha: check-alpha libs logUser
	cd hg && ${MAKE} alpha
	cd utils/bedToBigBed && ${MAKE} alpha

cgi-beta: check-beta libs logUser
	cd hg && ${MAKE} beta
	cd utils/bedToBigBed && ${MAKE} beta

# This log file can be useful when debugging problems on the alpha and beta builds.
logUser:
ifeq (${IS_HGWDEV},yes)
	echo ${USER}, $(MAKECMDGOALS), `date` >> ${CGI_BIN}/buildLog.txt
endif

check-alpha:
	# this will fail if we are not on master branch:
	git branch | egrep '\* master' > /dev/null

check-beta:
	# this will fail if we are not in a beta checkout:
	git branch | egrep '\* v[0-9]+_branch' > /dev/null	

BLAT_SUITE_DIRS = blat gfClient gfServer webBlat hg/pslPretty hg/pslReps \
	hg/pslSort utils/nibFrag utils/faToNib utils/faToTwoBit utils/twoBitToFa \
	utils/twoBitInfo isPcr blatz

blatSuite: ${BLAT_SUITE_DIRS:%=%.blatSuite}

%.blatSuite: topLibs hgLib
	 ${MAKE} -C $*

# all of these application makefiles have been updated to include use
#	of DESTDIR and BINDIR

# common subdirs uses by multiple targets
STD_DIRS = ameme index dnaDust protDust weblet aladdin primeMate fuse meta tagStorm \
	tabFile parasol blat

USER_APP_DIRS = utils parasol hg hg/utils \
	hg/makeDb tabFile
ALMOST_USER_APP_DIRS = blat gfClient gfServer isPcr ameme index/ixIxx utils/userApps

userApps: ${USER_APP_DIRS:%=%.userApps} ${ALMOST_USER_APP_DIRS:%=%.almostUserApps}

%.userApps: topLibs hgLib destBin
	${MAKE} -C $* userApps

# these dont have userApps targets
%.almostUserApps: topLibs hgLib destBin
	${MAKE} -C $*

destBin:
	${MKDIR} ${DESTBINDIR}

CLEAN_DIRS = ${STD_DIRS} lib hg jkOwnLib htslib utils webBat isPcr

clean: ${CLEAN_DIRS:%=%.clean}
	find . -name \*.o -print | xargs --no-run-if-empty rm
	rm -f tags TAGS
	rm -f cscope.out cscope.files cscope.po.out

%.clean:
	if [ -e "$*/makefile" ] ; then ${MAKE} -C $* clean ; fi


TEST_DIRS = blat gfServer hg lib utils
test:: $(TEST_DIRS:%=%.test)
%.test:
	if test -d $* ; then ${MAKE} -C $* test; fi

LIB_TAGS_IN = lib/*.[hc] */lib/*.[hc] */*/lib/*.[hc] */*/*/lib/*.[hc] jkOwnLib/*.c \
	inc/*.h */inc/*.h */*/inc/*.h */*/*/inc/*.h */cgilib/*.[hc]

# build tags for libraries
.PHONY: tags
tags:
	ctags ${LIB_TAGS_IN}

cscope.out:
	find `pwd` -name '*.c' -o -name '*.h' > cscope.files
	cscope -qRb `cat cscope.files`
search: cscope.out
	cscope -d

# build emacs tags for libraries
.PHONY: etags
etags:
	etags ${LIB_TAGS_IN}

# build tags for all files
.PHONY: tags-all
tags-all:
	find . -name '*.[ch]' | grep -v "/userApps/" | ctags -L -

# build emacs tags for all files
.PHONY: etags-all
etags-all:
	find . -name '*.[ch]' | etags -

UTILS_DIRS = ${DIRS} utils
.PHONY: utils
utils: ${UTILS_DIRS:%=%.utils} ${STD_DIRS:%=%.utils}

%.utils: libs destBin hgutils
	@${MKDIR} ${SCRIPTS}
	${MAKE} -C $*

utils-alpha: ${UTILS_DIRS:%=%.utils-alpha}

%.utils-alpha: check-alpha libs destBin hgutils
	if [ -e "$*/makefile" ] ; then ${MAKE} -C $* ; fi

hgutils: libs destBin
	cd hg && ${MAKE} utils

##  cellar archive for obsolete programs

cellarDirs = cdnaAli getgene idbQuery reformat scanIntrons tracks wormAli \
	xenoAli

buildCellar: $(cellarDirs:%=%.cellar)

%.cellar: libs destBin
	cd $* && echo $* && $(MAKE)

cleanCellar: $(cellarDirs:%=%.cellarClean)
%.cellarClean:
	cd $* && echo $* && $(MAKE) clean

## top level target for everything html related
DOCS_LIST = hg/htdocs hg/js hg/htdocs/style
doc: ${DOCS_LIST:%=%.docuser}
%.docuser:
	cd $* && $(MAKE)

doc-alpha: ${DOCS_LIST:%=%.docalpha}
%.docalpha:
	cd $* && $(MAKE) alpha

doc-beta: ${DOCS_LIST:%=%.docbeta}
%.docbeta:
	cd $* && $(MAKE) beta

doc-install: ${DOCS_LIST:%=%.docinstall}
%.docinstall:
	cd $* && $(MAKE) install
