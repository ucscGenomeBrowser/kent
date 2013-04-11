kentSrc = ../../..
A = txgToAgx
include ${kentSrc}/inc/userApp.mk

test::
	txgToAgx input.txg output.agx
	diff expected.agx output.agx

clean::
	rm -f output.agx
