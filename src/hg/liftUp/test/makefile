include ../../../inc/common.mk

all: chainQ chainT netQ netT

chainQ:
	$(HOME)/bin/$(MACHTYPE)/liftUp -chainQ testOutQ.chain \
            testInChainQ.lft warn testIn.chain
	diff testOutQ.chain testGoodQ.chain

chainT:
	$(HOME)/bin/$(MACHTYPE)/liftUp testOutT.chain \
            testInChainT.lft warn testIn.chain
	diff testOutT.chain testGoodT.chain

netQ:
	$(HOME)/bin/$(MACHTYPE)/liftUp -netQ testOutQ.net \
            testInNetQ.lft warn testIn.net
	diff testOutQ.net testGoodQ.net

netT:
	$(HOME)/bin/$(MACHTYPE)/liftUp testOutT.net \
            testInChainT.lft warn testIn.net
	diff testOutT.net testGoodT.net

