kentSrc = ../../..
A = txOrtho
include ${kentSrc}/inc/userApp.mk

test::
	txOrtho input.txg input.chain input.net inOrtho.txg output.edges -verbose=0
	diff expected.edges output.edges

clean::
	rm -f output.edges

