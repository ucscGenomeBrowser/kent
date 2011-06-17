include ../inc/common.mk

L += -lm $(SOCKETLIB)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a 

O = blat.o

blat: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/blat $O $(MYLIBS) $L
	${STRIP} ${DESTDIR}${BINDIR}/blat${EXE}

all:
	cd ../lib && ${MAKE}
	make
tags:
	ctags *.c *.h ../lib/*.c ../inc/*.h

test::
	blat -verbose=0 hCrea.geno hCrea.mrna testRna.psl
	cmp testRna.psl refRna.psl
	blat -verbose=0 -prot hCrea.pep mCrea.pep testProt.psl
	cmp testProt.psl refProt.psl
	blat -verbose=0 -t=dnax -q=prot hCrea.geno mCrea.pep testProtX.psl
	cmp testProtX.psl refProtX.psl
	blat -verbose=0 -t=dnax -q=rnax hCrea.geno mCrea.mrna testRnaX.psl
	cmp testRnaX.psl refRnaX.psl
	blat -verbose=0 -fine hCrea.geno hCrea.mrna testFine.psl
	cmp testFine.psl refFine.psl
	cd test && ${MAKE}

clean::
	rm -f testRna.psl testProt.psl testProtX.psl testRnaX.psl \
	testFine.psl $(O) blat
