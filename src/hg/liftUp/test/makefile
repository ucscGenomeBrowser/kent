include ../../../inc/common.mk

all: test

test: chainQ chainT netQ netT genePred pslRc

chainQ: output
	$(HOME)/bin/$(MACHTYPE)/liftUp -chainQ output/testOutQ.chain \
            input/testInChainQ.lft warn input/testIn.chain
	diff expected/testOutQ.chain output/testOutQ.chain

chainT: output
	$(HOME)/bin/$(MACHTYPE)/liftUp output/testOutT.chain \
            input/testInChainT.lft warn input/testIn.chain
	diff expected/testOutT.chain output/testOutT.chain

netQ: output
	$(HOME)/bin/$(MACHTYPE)/liftUp -netQ output/testOutQ.net \
            input/testInNetQ.lft warn input/testIn.net
	diff expected/testOutQ.net output/testOutQ.net

netT: output
	$(HOME)/bin/$(MACHTYPE)/liftUp output/testOutT.net \
            input/testInChainT.lft warn input/testIn.net
	diff expected/testOutT.net output/testOutT.net

genePred: output
	$(HOME)/bin/$(MACHTYPE)/liftUp output/testChrom.gp \
            input/contig.lft warn input/testContig.gp
	diff expected/testChrom.gp output/testChrom.gp

pslRc: output
	$(HOME)/bin/$(MACHTYPE)/liftUp output/rcLifted.psl \
	    input/rcTest.lft warn input/rcContig.psl
	diff expected/rcLifted.psl output/rcLifted.psl
	$(HOME)/bin/$(MACHTYPE)/liftUp output/rcRcLifted.psl \
	    input/rcTest.lft warn input/rcRcContig.psl
	diff expected/rcRcLifted.psl output/rcRcLifted.psl

chainRcT: output
	$(HOME)/bin/$(MACHTYPE)/liftUp output/rcLifted.chain \
	    input/rcTest.lft warn input/rcContig.chain
	diff expected/rcLifted.chain output/rcLifted.chain
	$(HOME)/bin/$(MACHTYPE)/liftUp output/rcRcLifted.chain \
	    input/rcTest.lft warn input/rcRcContig.chain
	diff expected/rcRcLifted.chain output/rcRcLifted.chain

output:
	${MKDIR} output

clean:
	rm -rf output

