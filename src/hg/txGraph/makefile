# Build txGraph source
include ../../inc/common.mk

all:    
	@for D in $(DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE)  ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done

DIRS = \
    agxToTxg \
    bedToTxEdges \
    txAbFragFind \
    txBedToGraph \
    txOrtho \
    txPslFilter \
    txPslToBed \
    txWalk \
    txgAddEvidence \
    txgAnalyze \
    txgGoodEdges \
    txgToAgx \
    txgToXml \
    txgTrim 
