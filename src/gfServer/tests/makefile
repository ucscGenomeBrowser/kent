kentSrc = ../..
include $(kentSrc)/inc/common.mk

# tests get assign different ports so they can run in parallel
gfServerHuge = gfServerHuge
ifeq (${IDXSIZE},huge)
gfServer = ${gfServerHuge}
portOffset = 1
else
gfServer = gfServerHuge
portOffset = 0
endif

test::
	${MAKE} test_idxsize
	${MAKE} test_idxsize IDXSIZE=huge
	@echo "Note: to run tests on a 40 gbase genome run: make hugeTests" 

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
	./bin/testProtNib ${gfServer} $(shell expr 17768 + ${portOffset})
protTwoBitTest: mkdirs
	./bin/testProtTwoBit ${gfServer} $(shell expr 17770 + ${portOffset})
transNibTest: mkdirs
	./bin/testTransNib ${gfServer} $(shell expr 17772 + ${portOffset})
transTwoBitTest: mkdirs
	./bin/testTransTwoBit ${gfServer} $(shell expr 17774 + ${portOffset})
pcrTest: mkdirs
	./bin/testPcr ${gfServer}
indexFileTest: mkdirs
	./bin/testIndexFile ${gfServer}  $(shell expr 17780 + ${portOffset})
indexFileTransTest: mkdirs
	./bin/testIndexFileTrans ${gfServer} $(shell expr 17782 + ${portOffset})
dynServerTest: mkdirs
	./bin/testDynServer ${gfServer}
dynServerTransTest: mkdirs
	./bin/testDynServerTrans ${gfServer}
dynServerWithDirTest: mkdirs
	./bin/testDynServerWithDir ${gfServer}
dynServerPerSeqMaxTest: mkdirs
	./bin/testDynServerPerSeqMax ${gfServer}

##
# tests using a huge genome requring 64-bit offsets
##
hugeAcc = GCF_019279795.1
hugeRelDir = GCF/019/279/795/${hugeAcc}
hugeSrcRoot = /hive/data/genomes/asmHubs
hugeSrcDir = ${hugeSrcRoot}/${hugeRelDir}
hugeSrc2bit = ${hugeSrcDir}/${hugeAcc}.2bit

hugeTestDir = output/huge/${hugeRelDir}
hugeTest2bit = ${hugeTestDir}/${hugeAcc}.2bit
hugeTestUntransIdx = ${hugeTestDir}/${hugeAcc}.untrans.gfidx
hugeTestTransIdx = ${hugeTestDir}/${hugeAcc}.trans.gfidx

# expensive test only run manually for now once indexs are deployed on hgwdev,
# this could be part of the standard run

hugeTests: testHugeUntrans testHugeTrans testHugeProt

testHugeUntrans: mkdirs ${hugeTestUntransIdx}
	./bin/runHugeTest ${gfServerHuge} 17790 untrans ${hugeTest2bit} ${hugeTestUntransIdx} input/lungfish.rna.fa $@

testHugeTrans: mkdirs ${hugeTestTransIdx}
	./bin/runHugeTest ${gfServerHuge} 17791 trans ${hugeTest2bit} ${hugeTestTransIdx} input/lungfish.rna.fa $@

testHugeProt: mkdirs ${hugeTestTransIdx}
	./bin/runHugeTest ${gfServerHuge} 17792 prot ${hugeTest2bit} ${hugeTestTransIdx} input/lungfish.prot.fa $@


# setup for huge tests
${hugeTestUntransIdx}: ${hugeTest2bit}
	${gfServerHuge} index $@ $<

${hugeTestTransIdx}: ${hugeTest2bit}
	${gfServerHuge} index -trans $@ $<

${hugeTest2bit}: ${hugeSrc2bit}
	@mkdir -p $(dir $@)
	ln -sf $< $@

mkdirs:
	${MKDIR} output


clean::
	rm -rf output
