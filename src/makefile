all: cgi

alpha: clean cgi-alpha blat

beta: clean cgi-beta

libs:
	cd lib; make; cd ..
	cd jkOwnLib; make; cd ..
	cd hg/lib; make; cd ../..

cgi: libs
	cd hg; make; cd ..

cgi-alpha: libs
	cd hg; make alpha; cd ..

cgi-beta: libs
	cd hg; make beta; cd ..

blat: libs
	cd blat; make; make test; cd ..
	cd gfClient; make; cd ..
	cd gfServer; make; cd ..
clean:
	cd lib; make clean; cd .. 
	cd hg; make clean; cd ..
	cd jkOwnLib; make clean; cd ..

tags:
	ctags $(HOME)/src/lib/*.c $(HOME)/src/inc/*.h $(HOME)/src/hg/lib/*.c $(HOME)/src/jkOwnLib/*.c

DIRS = aladdin ameme cdnaAli dnaDust fuzzyFind \
       getgene idbQuery index primeMate protDust reformat \
       scanIntrons tracks weblet wormAli xenoAli

intronerator: 
	cd lib; make; cd ..
	for i in $(DIRS); do cd $$i; make; cd ..; done; cd hg; make; cd ..;

