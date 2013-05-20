include inc/common.mk


all: utils cgi blatSuite

alpha: clean utils-alpha cgi-alpha blatSuite

beta: check-beta clean cgi-beta

# do a git update and clean
update:
	git pull
	${MAKE} clean

topLibs:
	@./machTest.sh
	cd lib && ${MAKE}
	cd jkOwnLib && ${MAKE}

hgLib:
	@./hg/sqlEnvTest.sh
	cd hg/lib && ${MAKE}

libs: topLibs hgLib

cgi: libs
	cd hg && ${MAKE} cgi

cgi-alpha: libs
	cd hg && ${MAKE} alpha

cgi-beta: check-beta libs
	cd hg && ${MAKE} beta

check-beta:
	# this will fail if we are not in a beta checkout:
	git branch | egrep 'v[0-9]+_branch' > /dev/null	

blatSuite: topLibs hgLib
	cd blat && ${MAKE}
	cd gfClient && ${MAKE}
	cd gfServer && ${MAKE}
	cd webBlat && ${MAKE}
	cd hg/pslPretty && ${MAKE}
	cd hg/pslReps && ${MAKE}
	cd hg/pslSort && ${MAKE}
	cd utils/nibFrag && ${MAKE}
	cd utils/faToNib && ${MAKE}
	cd utils/faToTwoBit && ${MAKE}
	cd utils/twoBitToFa && ${MAKE}
	cd utils/twoBitInfo && ${MAKE}
	cd isPcr && ${MAKE}
	cd blatz && ${MAKE}

# all of these application makefiles have been updated to include use
#	of DESTDIR and BINDIR

HG_APPLIST = bedSort liftUp liftOver bedItemOverlapCount encode/validateFiles \
	 featureBits ratStuff/mafsInRegion ratStuff/mafSpeciesSubset \
	 pslCDnaFilter genePredToGtf pslPretty pslReps pslSort 


HG_UTILS_APPLIST = bedExtendRanges gff3ToGenePred gtfToGenePred \
	hubCheck overlapSelect makeTableList pslMap

HG_HGWIGGLE_APP = hgWiggle

HG_LOAD_APPLIST = hgGcPercent

BLAT_APPLIST = blat gfClient gfServer

userApps: topLibs hgLib
	cd utils/stringify && echo utils/stringify && ${MAKE}
	cd utils && echo utils && ${MAKE} userApps
	cd utils/userApps && echo utils/userApps && ${MAKE}
	@for P in ${HG_APPLIST}; do \
	    ( cd hg/$${P} && echo hg/$${P} && ${MAKE} ) ; \
	done
	@for P in ${HG_UTILS_APPLIST}; do \
	    ( cd hg/utils/$${P} && echo hg/utils/$${P} && ${MAKE} ) ; \
	done
	@for P in ${HG_HGWIGGLE_APP}; do \
	    ( cd hg/makeDb/hgLoadWiggle && echo hg/makeDb/hgLoadWiggle && ${MAKE} $${P} ) ; \
	done
	@for P in ${HG_LOAD_APPLIST}; do \
	    ( cd hg/makeDb/$${P} && echo hg/makeDb/$${P} && ${MAKE} $${P} ) ; \
	done
	@for P in ${BLAT_APPLIST}; do \
	    ( cd $${P} && echo $${P} && ${MAKE} ) ; \
	done

clean:
	@for D in ${DIRS} x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && ${MAKE} clean ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done
	cd lib && ${MAKE} clean 
	cd hg && ${MAKE} clean
	cd hg && ${MAKE} clean_utils
	cd jkOwnLib && ${MAKE} clean
	cd utils && ${MAKE} clean
	cd webBlat && ${MAKE} clean
	cd isPcr && ${MAKE} clean
	touch non-empty-rm.o
	- find . -name \*.o -print | xargs rm
	rm -f tags TAGS

testDirs = lib blat gfServer hg
test:: $(testDirs:%=%.test)
%.test:
	cd $* && ${MAKE} test

LIB_TAGS_IN = lib/*.[hc] */lib/*.[hc] */*/lib/*.[hc] jkOwnLib/*.c inc/*.h hg/inc/*.h hg/encode3/encodeDataWarehouse/lib/*.c hg/encode3/encodeDataWarehouse/inc/*.h

# build tags for libraries
.PHONY: tags
tags:
	ctags ${LIB_TAGS_IN}

# build emacs tags for libraries
.PHONY: etags
etags:
	etags ${LIB_TAGS_IN}

# build tags for all files
.PHONY: tags-all
tags-all:
	find . -name '*.[ch]' | ctags -L -

# build emacs tags for all files
.PHONY: etags-all
etags-all:
	find . -name '*.[ch]' | etags -

utils: libs
	${MKDIR} ${SCRIPTS}
	${MKDIR} ${DESTDIR}${BINDIR}
	cd parasol && ${MAKE}
	@for D in ${DIRS} x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && ${MAKE} ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done
	cd utils && ${MAKE}
	cd hg && ${MAKE} utils

utils-alpha: libs
	@for D in ${DIRS} x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && ${MAKE} ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done
	cd utils && ${MAKE} alpha
	cd hg && ${MAKE} utils

DIRS = ameme blat index dnaDust protDust weblet aladdin primeMate fuse meta

##  cellar archive for obsolete programs

cellarDirs = cdnaAli getgene idbQuery reformat scanIntrons tracks wormAli \
	xenoAli

buildCellar: libs
	${MKDIR} ${DESTDIR}${BINDIR}
	@for D in ${cellarDirs} x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && ${MAKE} ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done

cleanCellar:
	@for D in ${cellarDirs} x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && ${MAKE} clean ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done
