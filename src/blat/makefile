include ../inc/makefile.h

L = -lm $(SOCKETLIB)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a 

O = blat.o

blat: $O $(MYLIBS)
	gcc -ggdb -o ../../bin/$(MACHTYPE)/blat $O $(MYLIBS) $L
	strip ../../bin/$(MACHTYPE)/blat

all:
	cd ../lib; make
	make
tags:
	ctags *.c *.h ../lib/*.c ../inc/*.h

test:
	blat hCrea.geno hCrea.mrna testRna.psl
	cmp testRna.psl refRna.psl
	blat -prot hCrea.pep mCrea.pep testProt.psl
	cmp testProt.psl refProt.psl
	blat -t=dnax -q=prot hCrea.geno mCrea.pep testProtX.psl
	cmp testProtX.psl refProtX.psl
	blat -t=dnax -q=rnax hCrea.geno mCrea.mrna testRnaX.psl
	cmp testRnaX.psl refRnaX.psl
