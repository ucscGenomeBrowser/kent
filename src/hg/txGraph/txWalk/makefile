kentSrc = ../../..
A = txWalk
include ${kentSrc}/inc/userApp.mk

test::
	txWalk input.txg input.weights 3 output.bed -evidence=output.ev
	diff expected.bed output.bed
	diff expected.ev output.ev

clean::
	rm -f output.bed output.ev
