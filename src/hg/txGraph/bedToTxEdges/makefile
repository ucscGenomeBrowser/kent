kentSrc = ../../..
A = bedToTxEdges
include ${kentSrc}/inc/userApp.mk

test::
	bedToTxEdges tests/simple.in tests/simple.out
	diff tests/simple.expected tests/simple.out
	bedToTxEdges tests/blocked.in tests/blocked.out
	diff tests/blocked.expected tests/blocked.out
