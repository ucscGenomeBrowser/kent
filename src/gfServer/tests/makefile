kentSrc = ../..
include $(kentSrc)/inc/common.mk

# tests get assign different ports so they can run in parallel
ifeq (${IDXSIZE},64)
gfServer = gfServer64
portOffset = 1
else
gfServer = gfServer
portOffset = 0
endif

test::
	${MAKE} test_idxsize
	${MAKE} test_idxsize IDXSIZE=64

# expensive test only run manually for now once indexs are deployed on hgwdev,
# this could be part of the standard run
testbig64::
	${MAKE} big64_tests IDXSIZE=64

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
		input/mCreaGeno.nib > output/$@.out
	diff expected/$@.out output/$@.out
directTwoBitTest: mkdirs
	${gfServer} direct input/mCrea.mrna input/creaGeno.2bit \
		>output/$@.out
	diff expected/$@.out output/$@.out
protNibTest: mkdirs
	./testProtNib ${gfServer} $(shell expr 17768 + ${portOffset})
protTwoBitTest: mkdirs
	./testProtTwoBit ${gfServer} $(shell expr 17770 + ${portOffset})
transNibTest: mkdirs
	./testTransNib ${gfServer} $(shell expr 17772 + ${portOffset})
transTwoBitTest: mkdirs
	./testTransTwoBit ${gfServer} $(shell expr 17774 + ${portOffset})
pcrTest: mkdirs
	./testPcr ${gfServer}
indexFileTest: mkdirs
	./testIndexFile ${gfServer}  $(shell expr 17780 + ${portOffset})
indexFileTransTest: mkdirs
	./testIndexFileTrans ${gfServer} $(shell expr 17782 + ${portOffset})
dynServerTest: mkdirs
	./testDynServer ${gfServer}
dynServerTransTest: mkdirs
	./testDynServerTrans ${gfServer}
dynServerWithDirTest: mkdirs
	./testDynServerWithDir ${gfServer}
dynServerPerSeqMaxTest: mkdirs
	./testDynServerPerSeqMax ${gfServer}


##
# tests using a huge genome 64 bit
##
big64_tests: build

mkdirs:
	${MKDIR} output


clean::
	rm -rf output
