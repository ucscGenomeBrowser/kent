include ../inc/common.mk

L = -lm $(SOCKETLIB)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a 

O = gfServer.o
X = gfServer

gfServer: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/$X $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/$X${EXE}

test:
	${MKDIR} tests/output
	$X direct tests/input/mCrea.mrna tests/input/hCreaGeno.nib \
		tests/input/mCreaGeno.nib > tests/output/testNib
	$X direct tests/input/mCrea.mrna tests/input/creaGeno.2bit \
		>tests/output/testTwoBit
	cd tests && ./testProtNib output/testProtNib
	cd tests && ./testProtTwoBit output/testProtTwoBit
	cd tests && ./testTransNib output/testTransNib
	cd tests && ./testTransTwoBit output/testTransTwoBit
	cd tests && ./testPcr output/testPcr
	diff -r -x CVS tests/expected tests/output
	rm -r tests/output
