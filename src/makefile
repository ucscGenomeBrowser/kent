all: cgi

alpha: clean cgi-alpha blat

beta: check-beta clean cgi-beta

libs:
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

blat: libs
	cd blat && ${MAKE} && ${MAKE} test
	cd gfClient && ${MAKE}
	cd gfServer && ${MAKE}
clean:
	cd lib && ${MAKE} clean 
	cd hg && ${MAKE} clean
	cd jkOwnLib && ${MAKE} clean

tags:
	ctags $(HOME)/src/lib/*.c $(HOME)/src/inc/*.h $(HOME)/src/hg/lib/*.c $(HOME)/src/jkOwnLib/*.c

utils: libs
	@for D in $(DIRS) x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && $(MAKE) ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done
	# cd hg && ${MAKE};

DIRS = aladdin ameme cdnaAli dnaDust fuzzyFind \
       getgene idbQuery index primeMate protDust reformat \
       scanIntrons tracks weblet wormAli xenoAli

