DIRS = aladdin ameme cdnaAli dnaDust fuzzyFind \
       getgene idbQuery index primeMate protDust reformat \
       scanIntrons tracks weblet wormAli xenoAli

all: libs
	for i in $(DIRS); do cd $$i; make; cd ..; done; cd hg; make; cd ..;

full: clean
	all

libs:
	cd lib; make; cd ..
	cd jkOwnLib; make; cd ..
	cd hg/lib; make; cd ../..

clean:
	cd lib; make clean; cd ..; 
	cd hg; make clean; cd ..; 
	cd jkOwnLib; make clean; cd ..

