DIRS = aladdin ameme cdnaAli dnaDust fuzzyFind \
       getgene idbQuery index primeMate protDust reformat \
       scanIntrons tracks weblet wormAli xenoAli

all:
	cd lib; make; cd ..
	for i in $(DIRS); do cd $$i; make; cd ..; done

libs:
	cd lib; make; cd ..
	cd hg/lib; make; cd ../..
