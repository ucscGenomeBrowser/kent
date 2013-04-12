kentSrc = ../../..
A = txCdsEvFromProtein
include ${kentSrc}/inc/userApp.mk

test::
	txCdsEvFromProtein inProtein.fa in.psl inTx.fa output.tce -unmapped=output.unmapped -dodgeStop=3
	diff expected.tce output.tce
	diff expected.unmapped output.unmapped
