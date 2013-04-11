kentSrc = ../../..
A = agxToTxg
include ${kentSrc}/inc/userApp.mk

test::
	agxToTxg input.agx output.txg
	diff expected.txg output.txg
