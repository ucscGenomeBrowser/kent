include ../../../inc/common.mk

all: test

test: chainQ chainT netQ netT genePred

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

output:
	mkdir -p output

clean:
	rm -rf output

