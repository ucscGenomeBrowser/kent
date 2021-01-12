kentSrc = ../..
include $(kentSrc)/inc/common.mk

ifeq (${IDXSIZE},64)
gfServer = gfServer64
else
gfServer = gfServer
endif

test::
	${MAKE} test_idxsize
	${MAKE} test_idxsize IDXSIZE=64

test_idxsize:
	${MKDIR} output
	${gfServer} direct input/mCrea.mrna input/hCreaGeno.nib \
		input/mCreaGeno.nib > output/testNib.out
	diff expected/testNib.out output/testNib.out
	${gfServer} direct input/mCrea.mrna input/creaGeno.2bit \
		>output/testTwoBit.out
	diff expected/testTwoBit.out output/testTwoBit.out
	./testProtNib ${gfServer}
	./testProtTwoBit ${gfServer}
	./testTransNib ${gfServer}
	./testTransTwoBit ${gfServer}
	./testPcr ${gfServer}
	./testIndexFile ${gfServer}
	./testIndexFileTrans ${gfServer}
	./testDynServer ${gfServer}
	./testDynServerTrans ${gfServer}
	./testDynServerWithDir ${gfServer}
	./testDynServerPerSeqMax ${gfServer}


clean::
	rm -rf output
