all: utils cgi blatSuite

alpha: clean cgi-alpha blatSuite

beta: check-beta clean cgi-beta

# do a cvs update and clean
update:
	cvs update -d -P
	${MAKE} clean

topLibs:
	@./machTest.sh
	cd lib && ${MAKE}
	cd jkOwnLib && ${MAKE}

hgLib:
	@./hg/sqlEnvTest.sh
	cd hg/lib && ${MAKE}

libs: topLibs hgLib
	cd hg/protein/lib && ${MAKE}

cgi: libs
	cd hg && ${MAKE} cgi

cgi-alpha: libs
	cd hg && ${MAKE} alpha

cgi-beta: check-beta libs
	cd hg && ${MAKE} beta

check-beta:
	# this will fail if we are not in a beta checkout:
	cvs status makefile | egrep 'v[0-9]+_branch' > /dev/null	

blatSuite: topLibs 
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

clean:
	cd lib && ${MAKE} clean 
	cd hg && ${MAKE} clean
	cd jkOwnLib && ${MAKE} clean
	cd utils && ${MAKE} clean
	touch non-empty-rm.o
	- find . -name \*.o -print | xargs rm
	rm -f tags TAGS

testDirs = lib blat gfServer hg
test:: $(testDirs:%=%.test)
%.test:
	cd $* && ${MAKE} test

LIB_TAGS_IN = lib/*.[hc] hg/lib/*.[hc] jkOwnLib/*.c inc/*.h hg/inc/*.h

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
	@for D in $(DIRS) x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && $(MAKE) ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done
	cd utils && ${MAKE}
	cd hg && ${MAKE} utils

DIRS = ameme blat cdnaAli index reformat xenoAli \
       dnaDust protDust idbQuery scanIntrons tracks weblet \
       aladdin getgene primeMate wormAli


