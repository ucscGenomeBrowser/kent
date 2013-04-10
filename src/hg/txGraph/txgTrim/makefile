kentSrc = ../../..
A = txgTrim
include ${kentSrc}/inc/userApp.mk

test::
	txgTrim input.txg input.weights 3.0 output.txg
	diff expected.txg output.txg
	txgTrim output.txg input.weights 3.0 output2.txg
	diff expected.txg output2.txg

clean::
	rm -f output.txg output2.txg
