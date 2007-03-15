include ../../../inc/common.mk

all: test

test: chainQ chainT netQ netT genePred pslRc \
	 missingWarn missingCarry missingDrop  missingError bed8 bed8version

chainQ: output
	${BINDIR}/liftUp -chainQ output/testOutQ.chain \
            input/testInChainQ.lft warn input/testIn.chain
	diff expected/testOutQ.chain output/testOutQ.chain

chainT: output
	${BINDIR}/liftUp output/testOutT.chain \
            input/testInChainT.lft warn input/testIn.chain
	diff expected/testOutT.chain output/testOutT.chain

netQ: output
	${BINDIR}/liftUp -netQ output/testOutQ.net \
            input/testInNetQ.lft warn input/testIn.net
	diff expected/testOutQ.net output/testOutQ.net

netT: output
	${BINDIR}/liftUp output/testOutT.net \
            input/testInChainT.lft warn input/testIn.net
	diff expected/testOutT.net output/testOutT.net

genePred: output
	${BINDIR}/liftUp output/testChrom.gp \
            input/contig.lft warn input/testContig.gp
	diff expected/testChrom.gp output/testChrom.gp

pslRc: output
	${BINDIR}/liftUp output/rcLifted.psl \
	    input/rcTest.lft warn input/rcContig.psl
	diff expected/rcLifted.psl output/rcLifted.psl
	${BINDIR}/liftUp output/rcRcLifted.psl \
	    input/rcTest.lft warn input/rcRcContig.psl
	diff expected/rcRcLifted.psl output/rcRcLifted.psl

chainRcT: output
	${BINDIR}/liftUp output/rcLifted.chain \
	    input/rcTest.lft warn input/rcContig.chain
	diff expected/rcLifted.chain output/rcLifted.chain
	${BINDIR}/liftUp output/rcRcLifted.chain \
	    input/rcTest.lft warn input/rcRcContig.chain
	diff expected/rcRcLifted.chain output/rcRcLifted.chain

chainRcQ: output
	${BINDIR}/liftUp output/rcSwapLifted.chain \
	    input/rcTest.lft warn input/rcContigSwap.chain -chainQ
	diff expected/rcSwapLifted.chain output/rcSwapLifted.chain
	${BINDIR}/liftUp output/rcRcSwapLifted.chain \
	    input/rcTest.lft warn input/rcRcContigSwap.chain -chainQ
	diff expected/rcRcSwapLifted.chain output/rcRcSwapLifted.chain

missingWarn: output
	${BINDIR}/liftUp output/missingWarn.psl \
	    input/rcTestMissing.lft warn input/rcContig.psl >output/missingWarn.err 2>&1
	diff expected/missingWarn.err output/missingWarn.err
	diff expected/missingWarn.psl output/missingWarn.psl

missingCarry: output
	${BINDIR}/liftUp output/missingCarry.psl \
	    input/rcTestMissing.lft carry input/rcContig.psl >output/missingCarry.err 2>&1
	diff expected/missingCarry.err output/missingCarry.err
	diff expected/missingCarry.psl output/missingCarry.psl

missingDrop: output
	${BINDIR}/liftUp output/missingDrop.psl \
	    input/rcTestMissing.lft drop input/rcContig.psl >output/missingDrop.err 2>&1
	diff expected/missingDrop.err output/missingDrop.err
	diff expected/missingDrop.psl output/missingDrop.psl

missingError: output
	if ${BINDIR}/liftUp output/missingError.psl \
	    input/rcTestMissing.lft error input/rcContig.psl >output/missingError.err 2>&1; then false; else true; fi
	diff expected/missingError.err output/missingError.err
	diff expected/missingError.psl output/missingError.psl

bed8: output
	${BINDIR}/liftUp -type=.bed8 output/$@.bed input/contig.lft error input/bed8contig.bed
	diff expected/$@.bed output/$@.bed

bed8version: output
	${BINDIR}/liftUp -type=.bed8 -ignoreVersions output/$@.bed input/contigNoVer.lft error input/bed8contig.bed
	diff expected/bed8.bed output/$@.bed

output:
	${MKDIR} output

clean:
	rm -rf output

