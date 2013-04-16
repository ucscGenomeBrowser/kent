kentSrc = ../../..
A = txgAddEvidence
include ${kentSrc}/inc/userApp.mk

test::
	txgAddEvidence input.txg input.bed esttt output.txg
	diff expected.txg output.txg

clean::
	rm -f output.txg
