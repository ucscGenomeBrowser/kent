all: cgi blatSuite utils

alpha: clean cgi-alpha blatSuite

beta: check-beta clean cgi-beta

topLibs:
	@if test "$$MACHTYPE x" = " x" ; then \
		echo "ERROR: missing MACHTYPE definition in environment" ; \
		echo "it should be something simple: i386 i686 sparc alpha x86_64 ppc, etc..." ; \
		exit 255 ; \
	else \
		echo $$MACHTYPE | grep -l "\-" > /dev/null 2>/dev/null ; \
		if test "$$?" = "0" ; then \
		echo "ERROR: can not use a MACHTYPE with - character" ; \
		echo "MACHTYPE is: '$$MACHTYPE'" ; \
		echo "it should be something simple: i386 i686 sparc alpha x86_64 ppc, etc..." ; \
		exit 255 ; \
		fi ; \
	fi ;
	cd lib && ${MAKE}
	cd jkOwnLib && ${MAKE}

libs: topLibs
	cd hg/lib && ${MAKE}
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
	cd blat && ${MAKE} && ${MAKE} test
	cd gfClient && ${MAKE}
	cd gfServer && ${MAKE}
	cd hg/pslPretty && ${MAKE}
	cd hg/pslReps && ${MAKE}
	cd hg/pslSort && ${MAKE}
	cd utils/nibFrag && ${MAKE}
	cd utils/faToNib && ${MAKE}
	cd utils/faToTwoBit && ${MAKE}
	cd utils/twoBitToFa && ${MAKE}
	cd utils/twoBitInfo && ${MAKE}
	cd isPcr && ${MAKE}

clean:
	cd lib && ${MAKE} clean 
	cd hg && ${MAKE} clean
	cd jkOwnLib && ${MAKE} clean
	cd utils && ${MAKE} clean
	touch non-empty-rm.o
	- find . -name \*.o -print | xargs rm

test::
	cd lib && ${MAKE} test
	cd blat && ${MAKE} test
	cd hg && ${MAKE} test
	cd utils && ${MAKE} test

tags:
	ctags ${HOME}/kent/src/lib/*.c ${HOME}/kent/src/inc/*.h ${HOME}/kent/src/hg/lib/*.c ${HOME}/kent/src/hg/inc/*.h ${HOME}/kent/src/jkOwnLib/*.c

utils: libs
	@for D in $(DIRS) x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && $(MAKE) ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done
	cd hg && ${MAKE} utils
	cd utils && ${MAKE}

DIRS = ameme blat cdnaAli dnaDust fuzzyFind \
       idbQuery index protDust reformat \
       scanIntrons tracks weblet xenoAli

BAD_DIRS = aladdin getgene primeMate wormAli

