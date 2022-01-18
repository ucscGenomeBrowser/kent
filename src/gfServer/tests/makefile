kentSrc = ../..
include $(kentSrc)/inc/common.mk

# tests use same port
.NOTPARALLEL:

ifeq (${IDXSIZE},64)
gfServer = gfServer64
else
gfServer = gfServer
endif

test::
	${MAKE} test_idxsize
	${MAKE} test_idxsize IDXSIZE=64

# recursive target based on index size.  Indvidual rules can
# be run for debugging
test_idxsize: \
    directNibTest \
    directTwoBitTest \
    protNibTest \
    protTwoBitTest \
    transNibTest \
    transTwoBitTest \
    pcrTest \
    indexFileTest \
    indexFileTransTest \
    dynServerTest \
    dynServerTransTest \
    dynServerWithDirTest \
    dynServerPerSeqMaxTest


directNibTest: mkdirs
	${gfServer} direct input/mCrea.mrna input/hCreaGeno.nib \
		input/mCreaGeno.nib > output/testNib.out
	diff expected/testNib.out output/testNib.out
directTwoBitTest: mkdirs
	${gfServer} direct input/mCrea.mrna input/creaGeno.2bit \
		>output/testTwoBit.out
	diff expected/testTwoBit.out output/testTwoBit.out
protNibTest: mkdirs
	./testProtNib ${gfServer}
protTwoBitTest: mkdirs
	./testProtTwoBit ${gfServer}
transNibTest: mkdirs
	./testTransNib ${gfServer}
transTwoBitTest: mkdirs
	./testTransTwoBit ${gfServer}
pcrTest: mkdirs
	./testPcr ${gfServer}
indexFileTest: mkdirs
	./testIndexFile ${gfServer}
indexFileTransTest: mkdirs
	./testIndexFileTrans ${gfServer}
dynServerTest: mkdirs
	./testDynServer ${gfServer}
dynServerTransTest: mkdirs
	./testDynServerTrans ${gfServer}
dynServerWithDirTest: mkdirs
	./testDynServerWithDir ${gfServer}
dynServerPerSeqMaxTest: mkdirs
	./testDynServerPerSeqMax ${gfServer}


mkdirs:
	${MKDIR} output


clean::
	rm -rf output
