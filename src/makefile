all: cgi

alpha: clean cgi-alpha blat

beta: check-beta clean cgi-beta

libs:
	cd lib && ${MAKE}
	cd jkOwnLib && ${MAKE}
	cd hg/lib && ${MAKE}

cgi: libs
	cd hg && ${MAKE}

cgi-alpha: libs
	cd hg && ${MAKE} clean

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

DIRS = aladdin ameme cdnaAli dnaDust fuzzyFind \
       getgene idbQuery index primeMate protDust reformat \
       scanIntrons tracks weblet wormAli xenoAli

intronerator: 
	cd lib && ${MAKE}
	for i in $(DIRS); do cd $$i && ${MAKE}; done;
	cd hg && ${MAKE};

