kentSrc = ../../..
A = txBedToGraph
extraObjects = makeGraph.o
include ${kentSrc}/inc/userApp.mk

test::
	txBedToGraph -verbose=0 inRef.bed refSeq input.bed mrna output.txg 
	diff expected.txg output.txg

clean::
	rm -f output.txg
