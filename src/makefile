all: cgi utils

alpha: clean cgi-alpha blat

beta: check-beta clean cgi-beta

libs:
	@if test "$$MACHTYPE x" = " x" ; then \
		echo "missing MACHTYPE definition in environment" ; \
		echo "it should be one of: i386 i686 sparc alpha x86_64 ppc" ; \
		exit 255 ; \
	else \
		echo $$MACHTYPE | grep -q -e- ; \
		if test "$$?" = "0" ; then \
		echo "can not use a MACHTYPE with - character" ; \
		echo "MACHTYPE: $$MACHTYPE" ; \
		echo "it should be one of: i386 i686 sparc alpha x86_64 ppc" ; \
		exit 255 ; \
		fi ; \
	fi ;
	cd lib && ${MAKE}
	cd jkOwnLib && ${MAKE}
	cd hg/lib && ${MAKE}

cgi: libs
	cd hg && ${MAKE} cgi

cgi-alpha: libs
	cd hg && ${MAKE} alpha

cgi-beta: check-beta libs
	cd hg && ${MAKE} beta

check-beta:
	# this will fail if we are not in a beta checkout:
	cvs status makefile | egrep 'v[0-9]+_branch' > /dev/null	

blatSuite: libs
	cd blat && ${MAKE} && ${MAKE} test
	cd gfClient && ${MAKE}
	cd gfServer && ${MAKE}
	cd hg/pslPretty && ${MAKE}
	cd hg/pslReps && ${MAKE}
	cd hg/pslSort && ${MAKE}
	cd utils/nibFrag && ${MAKE}
	cd utils/faToNib && ${MAKE}

clean:
	cd lib && ${MAKE} clean 
	cd hg && ${MAKE} clean
	cd jkOwnLib && ${MAKE} clean
	cd utils && ${MAKE} clean

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
	cd hg && ${MAKE} utils;

DIRS = ameme cdnaAli dnaDust fuzzyFind \
       idbQuery index protDust reformat \
       scanIntrons tracks weblet xenoAli

BAD_DIRS = aladdin getgene primeMate wormAli

