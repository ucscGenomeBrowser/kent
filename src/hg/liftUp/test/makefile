include ../../../inc/common.mk

all: chainQ chainT

chainQ:
	$(HOME)/bin/$(MACHTYPE)/liftUp -chainQ testOutQ.chain \
            testInChainQ.lft warn testIn.chain
	diff testOutQ.chain testGoodQ.chain

chainT:
	$(HOME)/bin/$(MACHTYPE)/liftUp testOutT.chain \
            testInChainT.lft warn testIn.chain
	diff testOutT.chain testGoodT.chain

