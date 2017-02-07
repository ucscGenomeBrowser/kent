kentSrc = ../../..
include ../../../inc/common.mk

liftUp = ${DESTBINDIR}/liftUp

all: test

test: chainQ chainT netQ netT genePred pslRc \
	 missingWarn missingCarry missingDrop  missingError bed8 bed8version \
	 bed3Strand

chainQ: output
	${liftUp} -chainQ output/testOutQ.chain \
            input/testInChainQ.lft warn input/testIn.chain
	diff expected/testOutQ.chain output/testOutQ.chain

chainT: output
	${liftUp} output/testOutT.chain \
            input/testInChainT.lft warn input/testIn.chain
	diff expected/testOutT.chain output/testOutT.chain

netQ: output
	${liftUp} -netQ output/testOutQ.net \
            input/testInNetQ.lft warn input/testIn.net
	diff expected/testOutQ.net output/testOutQ.net

netT: output
	${liftUp} output/testOutT.net \
            input/testInChainT.lft warn input/testIn.net
	diff expected/testOutT.net output/testOutT.net

genePred: output
	${liftUp} output/testChrom.gp \
            input/contig.lft warn input/testContig.gp
	diff expected/testChrom.gp output/testChrom.gp

pslRc: output
	${liftUp} output/rcLifted.psl \
	    input/rcTest.lft warn input/rcContig.psl
	diff expected/rcLifted.psl output/rcLifted.psl
	${liftUp} output/rcRcLifted.psl \
	    input/rcTest.lft warn input/rcRcContig.psl
	diff expected/rcRcLifted.psl output/rcRcLifted.psl

chainRcT: output
	${liftUp} output/rcLifted.chain \
	    input/rcTest.lft warn input/rcContig.chain
	diff expected/rcLifted.chain output/rcLifted.chain
	${liftUp} output/rcRcLifted.chain \
	    input/rcTest.lft warn input/rcRcContig.chain
	diff expected/rcRcLifted.chain output/rcRcLifted.chain

chainRcQ: output
	${liftUp} output/rcSwapLifted.chain \
	    input/rcTest.lft warn input/rcContigSwap.chain -chainQ
	diff expected/rcSwapLifted.chain output/rcSwapLifted.chain
	${liftUp} output/rcRcSwapLifted.chain \
	    input/rcTest.lft warn input/rcRcContigSwap.chain -chainQ
	diff expected/rcRcSwapLifted.chain output/rcRcSwapLifted.chain

missingWarn: output
	${liftUp} output/missingWarn.psl \
	    input/rcTestMissing.lft warn input/rcContig.psl >output/missingWarn.err 2>&1
	diff expected/missingWarn.err output/missingWarn.err
	diff expected/missingWarn.psl output/missingWarn.psl

missingCarry: output
	${liftUp} output/missingCarry.psl \
	    input/rcTestMissing.lft carry input/rcContig.psl >output/missingCarry.err 2>&1
	diff expected/missingCarry.err output/missingCarry.err
	diff expected/missingCarry.psl output/missingCarry.psl

missingDrop: output
	${liftUp} output/missingDrop.psl \
	    input/rcTestMissing.lft drop input/rcContig.psl >output/missingDrop.err 2>&1
	diff expected/missingDrop.err output/missingDrop.err
	diff expected/missingDrop.psl output/missingDrop.psl

missingError: output
	if ${liftUp} output/missingError.psl \
	    input/rcTestMissing.lft error input/rcContig.psl >output/missingError.err 2>&1; then false; else true; fi
	diff expected/missingError.err output/missingError.err
	diff expected/missingError.psl output/missingError.psl

bed8: output
	${liftUp} -type=.bed8 output/$@.bed input/contig.lft error input/bed8contig.bed
	diff expected/$@.bed output/$@.bed

bed8version: output
	${liftUp} -type=.bed8 -ignoreVersions output/$@.bed input/contigNoVer.lft error input/bed8contig.bed
	diff expected/bed8.bed output/$@.bed

bed3Strand: output
	${liftUp} -type=.bed3 output/$@.bed input/contigRevStrand.lft error input/bed8contig.bed
	diff expected/bed3Strand.bed output/$@.bed

output:
	${MKDIR} output

clean:
	rm -rf output

