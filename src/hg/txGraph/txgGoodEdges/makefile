kentSrc = ../../..
A = txgGoodEdges
include ${kentSrc}/inc/userApp.mk

test::
	txgGoodEdges input.txg input.weights 3 good output.edges
	diff expected.edges output.edges

clean::
	rm -f output.edges
