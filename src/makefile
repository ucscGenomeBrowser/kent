include inc/localEnvironment.mk
include inc/common.mk

all: utils cgi blatSuite

alpha: clean
	${MAKE} utils-alpha cgi-alpha blatSuite

beta: check-beta clean cgi-beta

# do a git update and clean
update:
	git pull
	${MAKE} clean

topLibs:
	@./checkUmask.sh
	@MACHTYPE=${MACHTYPE} ./machTest.sh
	cd lib && ${MAKE}
	cd jkOwnLib && ${MAKE}
	cd parasol/lib && ${MAKE}
	cd htslib && ${MAKE}

optLib:
	cd optimalLeaf && ${MAKE}

hgLib:
	@./hg/sqlEnvTest.sh
	cd hg/lib && ${MAKE}

libs: topLibs hgLib optLib

cgi: libs
	cd hg && ${MAKE} cgi
	cd utils/bedToBigBed && ${MAKE} cgi

cgi-alpha: libs
	cd hg && ${MAKE} alpha
	cd utils/bedToBigBed && ${MAKE} alpha

cgi-beta: check-beta libs
	cd hg && ${MAKE} beta
	cd utils/bedToBigBed && ${MAKE} beta

check-beta:
	# this will fail if we are not in a beta checkout:
	git branch | egrep '\* v[0-9]+_branch' > /dev/null	

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

BLAT_APPLIST = blat gfClient gfServer

userApps: topLibs hgLib destBin
	cd utils/stringify && echo utils/stringify && ${MAKE}
	cd utils && echo utils && ${MAKE} userAppsB
	cd parasol && echo parasol && ${MAKE} userApps
	cd hg && echo hg && ${MAKE} userApps
	cd hg/utils && echo hg/utils && ${MAKE} userApps
	cd hg/makeDb && echo hg/makeDb && ${MAKE} userApps
	cd ameme && echo ameme && ${MAKE}
	cd index/ixIxx && echo index/ixIxx && ${MAKE}
	@for P in ${BLAT_APPLIST}; do \
	    ( cd $${P} && echo $${P} && ${MAKE} ) ; \
	done
	cd utils/userApps && echo utils/userApps && ${MAKE}

destBin:
	${MKDIR} ${DESTBINDIR}

clean:
	@for D in ${DIRS} x; do \
            if test "$$D" != "x" && test -d "$$D" ; then \
                ( cd $$D && echo $$D && ${MAKE} clean ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done
	cd parasol && ${MAKE} clean
	cd lib && ${MAKE} clean 
	cd hg && ${MAKE} clean
	cd hg && ${MAKE} clean_utils
	cd jkOwnLib && ${MAKE} clean
	cd htslib && ${MAKE} clean
	cd utils && ${MAKE} clean
	if test -d webBlat ; then cd webBlat && ${MAKE} clean ; fi
	if test -d isPcr ; then cd isPcr && ${MAKE} clean ; fi
	touch non-empty-rm.o
	- find . -name \*.o -print | xargs rm
	rm -f tags TAGS
	rm -f cscope.out cscope.files cscope.po.out

testDirs = lib utils blat gfServer hg
test:: $(testDirs:%=%.test)
%.test:
	cd $* && ${MAKE} test

LIB_TAGS_IN = lib/*.[hc] */lib/*.[hc] */*/lib/*.[hc] */*/*/lib/*.[hc] jkOwnLib/*.c \
	inc/*.h */inc/*.h */*/inc/*.h */*/*/inc/*.h

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
	find . -name '*.[ch]' | ctags -L -

# build emacs tags for all files
.PHONY: etags-all
etags-all:
	find . -name '*.[ch]' | etags -

utils: libs destBin
	${MKDIR} ${SCRIPTS}
	cd parasol && ${MAKE}
	@for D in ${DIRS} x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && ${MAKE} ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done
	cd utils && ${MAKE}
	cd hg && ${MAKE} utils

utils-alpha: libs destBin
	${MKDIR} ${SCRIPTS}
	cd parasol && ${MAKE}
	@for D in ${DIRS} x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && ${MAKE} ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done
	cd utils && ${MAKE}
	cd hg && ${MAKE} utils

DIRS = ameme blat index dnaDust protDust weblet aladdin primeMate fuse meta tagStorm tabFile

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
